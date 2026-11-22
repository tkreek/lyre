#include "library.h"

#include <QJsonArray>
#include <QQmlEngine>
#include <algorithm>

#include "jellyfin.h"

namespace {
// Large enough that most libraries arrive in one request, small enough that
// a huge one starts drawing before the rest has downloaded.
constexpr int pageSize = 2000;
}

Library::Library(Jellyfin *jellyfin, QObject *parent) : QObject(parent), m_jellyfin(jellyfin) {
    connect(m_jellyfin, &Jellyfin::sessionChanged, this, [this] {
        clear();
        if (m_jellyfin->signedIn())
            loadAlbums();
    });
    if (m_jellyfin->signedIn())
        loadAlbums();
}

QList<Item> Library::parseItems(const QJsonDocument &json) {
    const QJsonArray array = json.isArray() ? json.array() : json.object().value("Items").toArray();
    QList<Item> items;
    items.reserve(array.size());
    for (const auto &value : array) {
        Item item = Item::fromJson(value.toObject());
        if (item.kind != Item::Unknown && !item.id.isEmpty())
            items << item;
    }
    return items;
}

// Only the fields the views draw, and one image tag per item, which keeps a
// several-thousand-album response to a few hundred kilobytes.
QUrlQuery Library::baseQuery() const {
    QUrlQuery query;
    query.addQueryItem("userId", m_jellyfin->userId());
    query.addQueryItem("Fields", "ProductionYear,ChildCount");
    query.addQueryItem("EnableUserData", "false");
    query.addQueryItem("EnableImageTypes", "Primary");
    query.addQueryItem("ImageTypeLimit", "1");
    return query;
}

void Library::fetchAll(ItemModel *model, const QString &path, QUrlQuery query, int start) {
    model->setLoading(true);
    if (start == 0)
        model->setError({});
    QUrlQuery page = query;
    page.addQueryItem("StartIndex", QString::number(start));
    page.addQueryItem("Limit", QString::number(pageSize));
    // A sign-out clears the models; comparing the user drops replies that
    // arrive for the previous one.
    const QString user = m_jellyfin->userId();
    m_jellyfin->get(path, page, model, [=, this](const QJsonDocument &json, const QString &error) {
        if (user != m_jellyfin->userId())
            return;
        if (!error.isEmpty()) {
            model->setError(error);
            model->setLoading(false);
            emit model->loaded();
            return;
        }
        const QList<Item> items = parseItems(json);
        if (start == 0)
            model->setItems(items);
        else
            model->insert(model->count(), items);
        if (json.isObject() && json.object().value("Items").toArray().size() == pageSize) {
            fetchAll(model, path, query, start + pageSize);
            return;
        }
        model->setLoading(false);
        emit model->loaded();
    });
}

void Library::loadAlbums(bool force) {
    if (!m_jellyfin->signedIn() || m_albums.loading() || (m_albums.count() && !force))
        return;
    QUrlQuery query = baseQuery();
    query.addQueryItem("IncludeItemTypes", "MusicAlbum");
    query.addQueryItem("Recursive", "true");
    query.addQueryItem("SortBy", "SortName");
    query.addQueryItem("SortOrder", "Ascending");
    fetchAll(&m_albums, "/Items", query);
}

void Library::loadArtists(bool force) {
    if (!m_jellyfin->signedIn() || m_artists.loading() || (m_artists.count() && !force))
        return;
    QUrlQuery query = baseQuery();
    query.addQueryItem("SortBy", "SortName");
    query.addQueryItem("SortOrder", "Ascending");
    fetchAll(&m_artists, "/Artists/AlbumArtists", query);
}

void Library::loadPlaylists(bool force) {
    if (!m_jellyfin->signedIn() || m_playlists.loading() || (m_playlists.count() && !force))
        return;
    QUrlQuery query = baseQuery();
    query.addQueryItem("IncludeItemTypes", "Playlist");
    query.addQueryItem("Recursive", "true");
    query.addQueryItem("SortBy", "SortName");
    fetchAll(&m_playlists, "/Items", query);
}

void Library::refresh() {
    loadAlbums(true);
    if (m_artists.count())
        loadArtists(true);
    if (m_playlists.count())
        loadPlaylists(true);
}

void Library::search(const QString &input) {
    const QString term = input.trimmed();
    if (term == m_searchTerm)
        return;
    m_searchTerm = term;
    emit searchTermChanged();
    const int serial = ++m_searchSerial;
    if (term.isEmpty()) {
        m_search.setItems({});
        m_search.setLoading(false);
        m_search.setError({});
        return;
    }
    QUrlQuery query = baseQuery();
    query.addQueryItem("searchTerm", term);
    query.addQueryItem("IncludeItemTypes", "MusicArtist,MusicAlbum,Audio,Playlist");
    query.addQueryItem("Recursive", "true");
    query.addQueryItem("Limit", "80");
    m_search.setLoading(true);
    m_search.setError({});
    m_jellyfin->get("/Items", query, this, [=, this](const QJsonDocument &json, const QString &error) {
        // Typing fires a search per pause; only the latest one may land.
        if (serial != m_searchSerial)
            return;
        m_search.setLoading(false);
        m_search.setError(error);
        QList<Item> items = parseItems(json);
        // Artists, then albums and playlists, then tracks, keeping the
        // server's relevance order within each group.
        auto rank = [](const Item &item) {
            switch (item.kind) {
            case Item::Artist: return 0;
            case Item::Album: return 1;
            case Item::Playlist: return 2;
            default: return 3;
            }
        };
        std::stable_sort(items.begin(), items.end(),
                         [&](const Item &a, const Item &b) { return rank(a) < rank(b); });
        m_search.setItems(items);
        emit m_search.loaded();
    });
}

ItemModel *Library::tracksOf(const QString &id, int kind) {
    auto *model = new ItemModel;
    QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
    QUrlQuery query = baseQuery();
    if (kind == Item::Playlist) {
        // Playlist order is the playlist's own, so no sorting here; video
        // entries in a mixed playlist are dropped by parseItems' kinds.
        fetchAll(model, "/Playlists/" + id + "/Items", query);
    } else if (kind == Item::Artist) {
        // An artist's whole discography, album by album in track order.
        query.addQueryItem("AlbumArtistIds", id);
        query.addQueryItem("IncludeItemTypes", "Audio");
        query.addQueryItem("Recursive", "true");
        query.addQueryItem("SortBy", "ProductionYear,Album,ParentIndexNumber,IndexNumber,SortName");
        fetchAll(model, "/Items", query);
    } else {
        query.addQueryItem("ParentId", id);
        query.addQueryItem("IncludeItemTypes", "Audio");
        query.addQueryItem("Recursive", "true");
        query.addQueryItem("SortBy", "ParentIndexNumber,IndexNumber,SortName");
        fetchAll(model, "/Items", query);
    }
    return model;
}

ItemModel *Library::albumsBy(const QString &artistId) {
    auto *model = new ItemModel;
    QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
    QUrlQuery query = baseQuery();
    query.addQueryItem("IncludeItemTypes", "MusicAlbum");
    query.addQueryItem("Recursive", "true");
    query.addQueryItem("AlbumArtistIds", artistId);
    query.addQueryItem("SortBy", "ProductionYear,SortName");
    query.addQueryItem("SortOrder", "Descending,Ascending");
    fetchAll(model, "/Items", query);
    return model;
}

void Library::clear() {
    for (auto *model : {&m_albums, &m_artists, &m_playlists, &m_search}) {
        model->setItems({});
        model->setLoading(false);
        model->setError({});
    }
    m_searchTerm.clear();
    ++m_searchSerial;
    emit searchTermChanged();
}
