#include "item.h"

#include <QJsonArray>

Item::Kind Item::kindFromType(const QString &type) {
    if (type == "Audio") return Track;
    if (type == "MusicAlbum") return Album;
    if (type == "MusicArtist") return Artist;
    if (type == "Playlist") return Playlist;
    return Unknown;
}

QString Item::artistLine() const {
    if (!artists.isEmpty())
        return artists.join(", ");
    return albumArtist;
}

Item Item::fromJson(const QJsonObject &o) {
    Item item;
    item.id = o.value("Id").toString();
    item.kind = kindFromType(o.value("Type").toString());
    item.name = o.value("Name").toString();
    for (const auto &artist : o.value("Artists").toArray())
        item.artists << artist.toString();
    item.albumArtist = o.value("AlbumArtist").toString();
    item.album = o.value("Album").toString();
    item.albumId = o.value("AlbumId").toString();
    item.year = o.value("ProductionYear").toInt();
    item.index = o.value("IndexNumber").toInt();
    item.disc = o.value("ParentIndexNumber").toInt();
    item.childCount = o.value("ChildCount").toInt();
    // Jellyfin counts time in 100 ns ticks.
    item.durationMs = static_cast<qint64>(o.value("RunTimeTicks").toDouble() / 10000.0);

    const QString ownTag = o.value("ImageTags").toObject().value("Primary").toString();
    if (!ownTag.isEmpty()) {
        item.imageId = item.id;
        item.imageTag = ownTag;
    }
    // A track's own embedded art is often missing or a low-resolution copy;
    // its album's cover is what the rest of the app shows, so prefer that.
    const QString albumTag = o.value("AlbumPrimaryImageTag").toString();
    if (item.kind == Track && !albumTag.isEmpty() && !item.albumId.isEmpty()) {
        item.imageId = item.albumId;
        item.imageTag = albumTag;
    }
    return item;
}
