#include "item.h"

#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>
#include <climits>

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
    // Some rips carry no track-number tags, only numbered file names
    // ("07 - Habanera.flac"); without this they'd sort alphabetically.
    if (item.kind == Track && item.index == 0) {
        static const QRegularExpression leading(R"(^\s*(\d{1,3})(?!\d))");
        const auto match = leading.match(o.value("Path").toString().section('/', -1));
        if (match.hasMatch())
            item.index = match.captured(1).toInt();
    }
    if (item.kind == Album) {
        item.discHint = discFromName(item.name);
        if (!item.discHint)
            item.discHint = discFromFolder(o.value("Path").toString().section('/', -1));
    }
    return item;
}

int Item::discFromName(const QString &name, QString *base) {
    static const QRegularExpression marker(
        R"(\s*[\(\[]?\s*(?:disc|disk|cd)\s*\.?\s*(\d+)(?:\s*(?:of|/)\s*\d+)?\s*[\)\]]?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    const auto match = marker.match(name);
    if (base)
        *base = match.hasMatch() ? name.left(match.capturedStart()).trimmed() : name.trimmed();
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

int Item::discFromFolder(const QString &folder) {
    static const QRegularExpression marker(R"((?:^|[^a-z])(?:disc|disk|cd)\s*\.?\s*(\d+))",
                                           QRegularExpression::CaseInsensitiveOption);
    const auto match = marker.match(folder);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

QList<Item> Item::sortAlbumTracks(QList<Item> tracks) {
    // Disc, then track number; a track with no number goes after the
    // numbered ones on its disc, and ties keep the server's order.
    auto number = [](const Item &item) { return item.index > 0 ? item.index : INT_MAX; };
    std::stable_sort(tracks.begin(), tracks.end(), [&](const Item &a, const Item &b) {
        if (a.disc != b.disc)
            return a.disc < b.disc;
        return number(a) < number(b);
    });
    return tracks;
}

QList<Item> Item::groupDiscSets(const QList<Item> &albums) {
    // Discs of a set share their name once the disc marker is gone. Some
    // rips credit a different artist per disc, so the name alone decides;
    // only when two discs claim the same number (two different "Greatest
    // Hits (Disc 1)") does the artist split them apart.
    auto keyOf = [](const Item &item) {
        QString base;
        discFromName(item.name, &base);
        return base.simplified().toLower();
    };
    QHash<QString, QList<int>> byName;
    QStringList order;
    for (int i = 0; i < albums.size(); ++i) {
        const QString key = keyOf(albums.at(i));
        if (!byName.contains(key))
            order << key;
        byName[key] << i;
    }

    QList<QList<int>> groups;
    for (const auto &key : std::as_const(order)) {
        const QList<int> rows = byName.value(key);
        QHash<int, int> numbers;
        bool clash = false;
        for (int row : rows)
            if (albums.at(row).discHint && ++numbers[albums.at(row).discHint] > 1)
                clash = true;
        // A name shared by albums that don't look like discs at all (two
        // artists' "Greatest Hits") stays apart, unless the artists match.
        bool anyDisc = false;
        for (int row : rows)
            anyDisc = anyDisc || albums.at(row).discHint;
        if (rows.size() > 1 && (clash || !anyDisc)) {
            QHash<QString, QList<int>> byArtist;
            QStringList artists;
            for (int row : rows) {
                const QString artist = albums.at(row).albumArtist.simplified().toLower();
                if (!byArtist.contains(artist))
                    artists << artist;
                byArtist[artist] << row;
            }
            for (const auto &artist : std::as_const(artists))
                groups << byArtist.value(artist);
        } else {
            groups << rows;
        }
    }

    QList<Item> out;
    out.reserve(groups.size());
    for (QList<int> rows : std::as_const(groups)) {
        if (rows.size() == 1) {
            out << albums.at(rows.first());
            continue;
        }
        // Disc order by number; an unnumbered disc goes first (it's
        // usually disc 1 of a rip that only marked the later ones).
        std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) {
            return albums.at(a).discHint < albums.at(b).discHint;
        });
        const Item &first = albums.at(rows.first());
        Item set = first;
        discFromName(first.name, &set.name);
        set.childCount = 0;
        set.members.clear();
        for (int row : std::as_const(rows)) {
            const Item &disc = albums.at(row);
            set.members << disc.id;
            set.childCount += disc.childCount;
            if (set.imageTag.isEmpty() && !disc.imageTag.isEmpty()) {
                set.imageId = disc.imageId;
                set.imageTag = disc.imageTag;
            }
            if (!set.year && disc.year)
                set.year = disc.year;
        }
        out << set;
    }
    return out;
}
