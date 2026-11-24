#pragma once

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <functional>

#include "artcache.h"
#include "queue.h"

class Jellyfin;
class Library;

// Plays the queue through Qt Multimedia's FFmpeg backend and tells the
// server what's playing, so play counts, "Continue listening", and the
// dashboard's Now Playing all work as with Jellyfin's own clients.
class Player : public QObject {
    Q_OBJECT
    Q_PROPERTY(ItemModel *queue READ queueModel CONSTANT)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY currentChanged)
    Q_PROPERTY(QString currentId READ currentId NOTIFY currentChanged)
    Q_PROPERTY(QString title READ title NOTIFY currentChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY currentChanged)
    Q_PROPERTY(QString album READ album NOTIFY currentChanged)
    Q_PROPERTY(QString albumId READ albumId NOTIFY currentChanged)
    Q_PROPERTY(QString imageId READ imageId NOTIFY currentChanged)
    Q_PROPERTY(QString imageTag READ imageTag NOTIFY currentChanged)
    Q_PROPERTY(QUrl artFile READ artFile NOTIFY artFileChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY currentChanged)
    Q_PROPERTY(bool canPrevious READ hasTrack NOTIFY currentChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int repeat READ repeat WRITE setRepeat NOTIFY repeatChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    Player(Jellyfin *jellyfin, Library *library, QObject *parent = nullptr);
    ~Player() override;

    Queue *queue() { return &m_queue; }
    ItemModel *queueModel() { return m_queue.model(); }
    int currentIndex() const { return m_queue.currentIndex(); }
    bool hasTrack() const { return m_queue.currentIndex() >= 0; }
    QString currentId() const { return m_queue.current().id; }
    QString title() const { return m_queue.current().name; }
    QString artist() const { return m_queue.current().artistLine(); }
    QStringList artists() const;
    QString album() const { return m_queue.current().album; }
    QString albumId() const { return m_queue.current().albumId; }
    QString imageId() const { return m_queue.current().imageId; }
    QString imageTag() const { return m_queue.current().imageTag; }
    QUrl artFile() const { return m_artFile; }
    bool canNext() const { return m_queue.nextIndex(false) >= 0; }
    bool playing() const { return m_media.playbackState() == QMediaPlayer::PlayingState; }
    // MPRIS's three states.
    QString status() const;
    bool buffering() const { return m_buffering; }
    qint64 position() const { return m_resumeAt >= 0 ? m_resumeAt : m_media.position(); }
    qint64 duration() const;
    double volume() const { return m_volume; }
    bool muted() const { return m_output.isMuted(); }
    bool shuffle() const { return m_queue.shuffle(); }
    int repeat() const { return m_queue.repeat(); }
    QString error() const { return m_error; }

    void setVolume(double volume);
    void setMuted(bool muted);
    void setShuffle(bool shuffle);
    void setRepeat(int repeat);

    // Replace the queue with a model's tracks and start at one of them.
    Q_INVOKABLE void playFrom(ItemModel *model, int index);
    Q_INVOKABLE void addToQueue(ItemModel *model, int index, bool next = false);
    // The same for an album, playlist, or artist that hasn't been opened:
    // its tracks are fetched first.
    // A set of discs passes its discs' album ids as `members`.
    Q_INVOKABLE void playContainer(const QString &id, int kind, bool shuffle = false,
                                   const QStringList &members = {});
    Q_INVOKABLE void addContainer(const QString &id, int kind, bool next = false,
                                  const QStringList &members = {});

    Q_INVOKABLE void playAt(int row);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void moveRow(int from, int to);
    Q_INVOKABLE void clearQueue();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 ms);
    Q_INVOKABLE void seekBy(qint64 ms);
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void clearError();

signals:
    void currentChanged();
    void artFileChanged();
    void stateChanged();
    void bufferingChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void shuffleChanged();
    void repeatChanged();
    void errorChanged();
    // A jump rather than playback moving along, for MPRIS's Seeked signal.
    void seeked(qint64 ms);
    // Tracks were added to the queue, for a brief confirmation in the window.
    void queued(int count);

private:
    void go(int row, bool autoplay);
    void loadCurrent(bool autoplay);
    void reopen(qint64 at, bool autoplay);
    bool recover();
    void advance(bool automatic);
    void withTracks(const QString &id, int kind, const QStringList &members,
                    std::function<void(const QList<Item> &)> use);
    void updateArt();
    void setError(const QString &error);
    void reportStart();
    void reportProgress(const QString &event);
    void reportStopped();
    QJsonObject report() const;

    Jellyfin *m_jellyfin;
    Library *m_library;
    Queue m_queue;
    QMediaPlayer m_media;
    QAudioOutput m_output;
    QMediaDevices m_devices;
    ArtCache m_art;
    QUrl m_artFile;
    QString m_loadedId;
    QString m_playSessionId;
    bool m_reported = false;
    bool m_switching = false;
    bool m_buffering = false;
    double m_volume = 1.0;
    QString m_error;
    QTimer m_progressTimer;
    // Where a reopened stream picks up once it has loaded, else -1.
    qint64 m_resumeAt = -1;
    // How long playback has been paused, and since the last reconnection.
    QElapsedTimer m_pausedFor;
    QElapsedTimer m_lastRecovery;
};
