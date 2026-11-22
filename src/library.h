#pragma once

#include <QObject>
#include <QUrlQuery>
#include <functional>
#include <memory>

#include "itemmodel.h"

class Jellyfin;

// The music library on the server, as models the window binds to. The big
// lists load once, in pages, the first time they're shown; an album's or
// playlist's tracks load when it's opened.
class Library : public QObject {
    Q_OBJECT
    Q_PROPERTY(ItemModel *albums READ albums CONSTANT)
    Q_PROPERTY(ItemModel *artists READ artists CONSTANT)
    Q_PROPERTY(ItemModel *playlists READ playlists CONSTANT)
    Q_PROPERTY(ItemModel *searchResults READ searchResults CONSTANT)
    Q_PROPERTY(QString searchTerm READ searchTerm NOTIFY searchTermChanged)

public:
    explicit Library(Jellyfin *jellyfin, QObject *parent = nullptr);

    ItemModel *albums() { return &m_albums; }
    ItemModel *artists() { return &m_artists; }
    ItemModel *playlists() { return &m_playlists; }
    ItemModel *searchResults() { return &m_search; }
    QString searchTerm() const { return m_searchTerm; }

    Q_INVOKABLE void loadAlbums(bool force = false);
    Q_INVOKABLE void loadArtists(bool force = false);
    Q_INVOKABLE void loadPlaylists(bool force = false);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void search(const QString &term);

    // A fresh model for one container, owned by the QML page that asked for
    // it, so going back frees it.
    Q_INVOKABLE ItemModel *tracksOf(const QString &id, int kind);
    Q_INVOKABLE ItemModel *albumsBy(const QString &artistId);
    // Every track of a set of discs, disc by disc.
    Q_INVOKABLE ItemModel *tracksOfSet(const QStringList &albumIds);
    // The album, or set of discs, that an album id belongs to, as a row of
    // the albums list; empty if it isn't loaded.
    Q_INVOKABLE QVariantMap setContaining(const QString &albumId) const;

    static QList<Item> orderSetTracks(const QStringList &albumIds, QList<Item> tracks);

    // Jellyfin returns a page as {"Items": [...]}; some endpoints return a
    // bare array. Both parse here, keeping only kinds a music player shows.
    static QList<Item> parseItems(const QJsonDocument &json);

signals:
    void searchTermChanged();

private:
    using Transform = std::function<QList<Item>(const QList<Item> &)>;
    void fetchAll(ItemModel *model, const QString &path, QUrlQuery query, Transform transform = {});
    void fetchPage(ItemModel *model, const QString &path, const QUrlQuery &query, Transform transform,
                   std::shared_ptr<QList<Item>> raw, int start);
    QUrlQuery baseQuery() const;
    void clear();

    Jellyfin *m_jellyfin;
    ItemModel m_albums, m_artists, m_playlists, m_search;
    QString m_searchTerm;
    int m_searchSerial = 0;
};
