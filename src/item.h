#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>

// One Jellyfin library item, trimmed to what a music player shows: an album,
// an artist, a playlist, or a track. Everything else in the server's JSON is
// dropped at parse time so models stay small.
struct Item {
    enum Kind { Unknown, Track, Album, Artist, Playlist };

    QString id;
    Kind kind = Unknown;
    QString name;
    QStringList artists;   // track artists, or empty
    QString albumArtist;
    QString album;
    QString albumId;
    int year = 0;
    int index = 0;         // track number on its disc
    int disc = 0;
    int childCount = 0;
    qint64 durationMs = 0;
    // The item whose Primary image to show and that image's tag, which also
    // busts the cache when the art changes. Tracks borrow their album's.
    QString imageId;
    QString imageTag;
    // An album's disc number when its name or folder says it's one disc of
    // a set ("Carmen (Disc 2)", ".../Carmen CD 2"), else 0.
    int discHint = 0;
    // For a set of discs shown as one album: every disc's album id, in disc
    // order. Empty for an ordinary item.
    QStringList members;

    // The artist line under a title: the track's artists, else the album's.
    QString artistLine() const;

    static Item fromJson(const QJsonObject &object);
    static Kind kindFromType(const QString &type);

    // "Carmen (Disc 2)" → 2, and the name without the marker in `base`.
    static int discFromName(const QString &name, QString *base = nullptr);
    // A folder named like "Carmen CD 2" or "... [CD2.2]" → 2.
    static int discFromFolder(const QString &folder);
    // Albums that are discs of one set become one album whose members are
    // the discs in order; everything else passes through unchanged.
    static QList<Item> groupDiscSets(const QList<Item> &albums);
    // An album's tracks in disc and track order.
    static QList<Item> sortAlbumTracks(QList<Item> tracks);
};

Q_DECLARE_METATYPE(Item)
