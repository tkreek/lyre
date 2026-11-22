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

    // The artist line under a title: the track's artists, else the album's.
    QString artistLine() const;

    static Item fromJson(const QJsonObject &object);
    static Kind kindFromType(const QString &type);
};

Q_DECLARE_METATYPE(Item)
