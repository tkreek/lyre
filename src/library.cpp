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

void Library::fetchAll(ItemModel *model, const QString &path, QUrlQuery query, Transform transform) {
    model->setLoading(true);
    model->setError({});
    fetchPage(model, path, query, std::move(transform), std::make_shared<QList<Item>>(), 0);
}

// Pages accumulate in `raw`, and each one redraws the model from all of it,
// so a transform like grouping discs sees every album fetched so far.
void Library::fetchPage(ItemModel *model, const QString &path, const QUrlQuery &query, Transform transform,
                        std::shared_ptr<QList<Item>> raw, int start) {
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
        *raw += parseItems(json);
        model->setItems(transform ? transform(*raw) : *raw);
        if (json.isObject() && json.object().value("Items").toArray().size() == pageSize) {
            fetchPage(model, path, query, transform, raw, start + pageSize);
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
    query.removeAllQueryItems("Fields");
    query.addQueryItem("Fields", "ProductionYear,ChildCount,Path");
    fetchAll(&m_albums, "/Items", query, &Item::groupDiscSets);
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
    query.removeAllQueryItems("Fields");
    query.addQueryItem("Fields", "ProductionYear,ChildCount,Path");
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
        // The discs of a set found by one search show as the set.
        QList<Item> albums, rest;
        for (const auto &item : std::as_const(items))
            (item.kind == Item::Album ? albums : rest) << item;
        int firstAlbum = 0;
        while (firstAlbum < items.size() && rank(items.at(firstAlbum)) < 1)
            ++firstAlbum;
        items = rest.mid(0, firstAlbum) + Item::groupDiscSets(albums) + rest.mid(firstAlbum);
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
        query.removeAllQueryItems("Fields");
        query.addQueryItem("Fields", "ProductionYear,ChildCount,Path");
        fetchAll(model, "/Items", query, &Item::sortAlbumTracks);
    }
    return model;
}

ItemModel *Library::tracksOfSet(const QStringList &albumIds) {
    if (albumIds.size() == 1)
        return tracksOf(albumIds.first(), Item::Album);
    auto *model = new ItemModel;
    QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
    QUrlQuery query = baseQuery();
    query.addQueryItem("AlbumIds", albumIds.join(','));
    query.addQueryItem("IncludeItemTypes", "Audio");
    query.addQueryItem("Recursive", "true");
    query.addQueryItem("SortBy", "ParentIndexNumber,IndexNumber,SortName");
    query.removeAllQueryItems("Fields");
    query.addQueryItem("Fields", "ProductionYear,ChildCount,Path");
    fetchAll(model, "/Items", query, [albumIds](const QList<Item> &tracks) {
        return orderSetTracks(albumIds, tracks);
    });
    return model;
}

// Each album of a set is one disc: its tracks are numbered as that disc and
// follow the discs before it, whatever disc tags the files carry.
QList<Item> Library::orderSetTracks(const QStringList &albumIds, QList<Item> tracks) {
    for (auto &track : tracks) {
        const int at = albumIds.indexOf(track.albumId);
        track.disc = at < 0 ? albumIds.size() + 1 : at + 1;
    }
    return Item::sortAlbumTracks(tracks);
}

QVariantMap Library::setContaining(const QString &albumId) const {
    for (int row = 0; row < m_albums.count(); ++row) {
        const Item &album = m_albums.items().at(row);
        if (album.id == albumId || album.members.contains(albumId))
            return m_albums.get(row);
    }
    return {};
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
    query.removeAllQueryItems("Fields");
    query.addQueryItem("Fields", "ProductionYear,ChildCount,Path");
    fetchAll(model, "/Items", query, &Item::groupDiscSets);
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
