#include "player.h"

#include <QAudio>
#include <QAudioDevice>
#include <QJsonObject>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QUuid>

#include "jellyfin.h"
#include "library.h"

namespace {
// Jellyfin counts time in 100 ns ticks.
qint64 ticks(qint64 ms) { return ms * 10000; }
// Past this far into a track, Previous restarts it instead of going back.
constexpr qint64 restartThresholdMs = 3000;
}

Player::Player(Jellyfin *jellyfin, Library *library, QObject *parent)
    : QObject(parent), m_jellyfin(jellyfin), m_library(library), m_art(jellyfin->network()) {
    m_media.setAudioOutput(&m_output);
    // Qt opens the stream on one named output and tells PipeWire not to move
    // it, so switching speakers in the sound settings would leave Lyre
    // playing where it started. Follow the system's default output instead.
    m_output.setDevice(QMediaDevices::defaultAudioOutput());
    connect(&m_devices, &QMediaDevices::audioOutputsChanged, this, [this] {
        const QAudioDevice device = QMediaDevices::defaultAudioOutput();
        if (device != m_output.device())
            m_output.setDevice(device);
    });

    connect(&m_media, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            if (!m_reported)
                reportStart();
            else
                reportProgress("Unpause");
            m_progressTimer.start();
        } else {
            m_progressTimer.stop();
            if (state == QMediaPlayer::PausedState)
                reportProgress("Pause");
        }
        emit stateChanged();
    });
    connect(&m_media, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        const bool buffering = status == QMediaPlayer::LoadingMedia || status == QMediaPlayer::BufferingMedia ||
                               status == QMediaPlayer::StalledMedia;
        if (buffering != m_buffering) {
            m_buffering = buffering;
            emit bufferingChanged();
        }
        if (status == QMediaPlayer::EndOfMedia)
            advance(true);
    });
    connect(&m_media, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &text) {
        if (error == QMediaPlayer::NoError)
            return;
        setError(tr("Couldn't play “%1”: %2").arg(title(), text));
    });
    connect(&m_media, &QMediaPlayer::positionChanged, this, &Player::positionChanged);
    connect(&m_media, &QMediaPlayer::durationChanged, this, &Player::durationChanged);

    connect(&m_queue, &Queue::currentChanged, this, [this] {
        // Removing the playing track changes the current row without going
        // through go(); a shuffle moves it but keeps the same track.
        if (!m_switching && currentId() != m_loadedId)
            loadCurrent(playing() || m_buffering);
        emit currentChanged();
    });
    connect(&m_queue, &Queue::shuffleChanged, this, &Player::shuffleChanged);
    connect(&m_queue, &Queue::repeatChanged, this, &Player::repeatChanged);
    connect(&m_queue, &Queue::repeatChanged, this, &Player::currentChanged);
    connect(m_queue.model(), &ItemModel::countChanged, this, &Player::currentChanged);

    connect(&m_art, &ArtCache::ready, this, [this](const QString &id, const QString &tag, const QUrl &file) {
        if (id == imageId() && tag == imageTag() && file != m_artFile) {
            m_artFile = file;
            emit artFileChanged();
        }
    });

    m_progressTimer.setInterval(10000);
    connect(&m_progressTimer, &QTimer::timeout, this, [this] { reportProgress("TimeUpdate"); });

    // A sign-out stops playback, since the stream's token stops working.
    connect(m_jellyfin, &Jellyfin::sessionChanged, this, [this] {
        if (!m_jellyfin->signedIn())
            clearQueue();
    });
}

Player::~Player() {
    reportStopped();
}

QStringList Player::artists() const {
    const Item item = m_queue.current();
    return item.artists.isEmpty() && !item.albumArtist.isEmpty() ? QStringList{item.albumArtist} : item.artists;
}

QString Player::status() const {
    switch (m_media.playbackState()) {
    case QMediaPlayer::PlayingState: return "Playing";
    case QMediaPlayer::PausedState: return "Paused";
    default: return "Stopped";
    }
}

// The server's runtime until the decoder knows better.
qint64 Player::duration() const {
    const qint64 decoded = m_media.duration();
    return decoded > 0 ? decoded : m_queue.current().durationMs;
}

void Player::setVolume(double volume) {
    volume = qBound(0.0, volume, 1.0);
    if (qFuzzyCompare(volume + 1, m_volume + 1))
        return;
    m_volume = volume;
    // The slider is perceptual; the audio output wants linear gain.
    m_output.setVolume(QAudio::convertVolume(volume, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale));
    emit volumeChanged();
}

void Player::setMuted(bool muted) {
    if (muted == m_output.isMuted())
        return;
    m_output.setMuted(muted);
    emit volumeChanged();
}

void Player::setShuffle(bool shuffle) {
    m_queue.setShuffle(shuffle);
}

void Player::setRepeat(int repeat) {
    m_queue.setRepeat(Queue::Repeat(qBound(0, repeat, 2)));
}

void Player::cycleRepeat() {
    setRepeat((repeat() + 1) % 3);
}

void Player::playFrom(ItemModel *model, int index) {
    if (!model || model->count() == 0)
        return;
    // Search results mix albums and artists in with tracks; only the tracks
    // go in the queue, starting from the one picked.
    QList<Item> tracks;
    int start = 0;
    for (int i = 0; i < model->count(); ++i) {
        if (model->at(i).kind != Item::Track)
            continue;
        if (i == index)
            start = tracks.size();
        tracks << model->at(i);
    }
    if (tracks.isEmpty())
        return;
    m_switching = true;
    m_queue.replace(tracks, start);
    m_switching = false;
    loadCurrent(true);
}

void Player::addToQueue(ItemModel *model, int index, bool next) {
    if (!model || model->count() == 0)
        return;
    const QList<Item> items = index < 0 ? model->items() : QList<Item>{model->at(index)};
    const bool wasEmpty = m_queue.count() == 0;
    if (next)
        m_queue.insertNext(items);
    else
        m_queue.append(items);
    emit queued(items.size());
    // Adding to an empty queue gives it something to play.
    if (wasEmpty)
        go(0, false);
}

void Player::withTracks(const QString &id, int kind, const QStringList &members,
                        std::function<void(const QList<Item> &)> use) {
    // The model is the player's while it loads, so a page closing or the
    // QML garbage collector can't take it away first.
    ItemModel *model = members.size() > 1 ? m_library->tracksOfSet(members) : m_library->tracksOf(id, kind);
    QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
    model->setParent(this);
    auto done = [this, model, use] {
        if (!model->error().isEmpty())
            setError(model->error());
        else if (model->count() == 0)
            setError(tr("There's nothing to play there."));
        else
            use(model->items());
        model->deleteLater();
    };
    if (model->loading())
        connect(model, &ItemModel::loaded, this, done, Qt::SingleShotConnection);
    else
        done();
}

void Player::playContainer(const QString &id, int kind, bool shuffle, const QStringList &members) {
    withTracks(id, kind, members, [this, shuffle](const QList<Item> &items) {
        // With shuffle requested, start from a random track and let the
        // queue shuffle the rest around it.
        m_switching = true;
        if (shuffle)
            m_queue.setShuffle(false);
        m_queue.replace(items, shuffle ? int(QRandomGenerator::global()->bounded(int(items.size()))) : 0);
        if (shuffle)
            m_queue.setShuffle(true);
        m_switching = false;
        loadCurrent(true);
    });
}

void Player::addContainer(const QString &id, int kind, bool next, const QStringList &members) {
    withTracks(id, kind, members, [this, next](const QList<Item> &items) {
        ItemModel model;
        model.setItems(items);
        addToQueue(&model, -1, next);
    });
}

void Player::playAt(int row) {
    if (row < 0 || row >= m_queue.count())
        return;
    go(row, true);
}

// Every deliberate move to another track comes through here, so the queue's
// currentChanged handler doesn't load it a second time.
void Player::go(int row, bool autoplay) {
    m_switching = true;
    m_queue.setCurrent(row);
    m_switching = false;
    loadCurrent(autoplay);
}

void Player::removeAt(int row) {
    m_queue.remove(row);
    if (m_queue.count() == 0)
        stop();
}

void Player::moveRow(int from, int to) {
    m_queue.move(from, to);
    emit currentChanged();
}

void Player::clearQueue() {
    reportStopped();
    m_media.stop();
    m_media.setSource({});
    m_loadedId.clear();
    m_switching = true;
    m_queue.clear();
    m_switching = false;
    updateArt();
}

void Player::loadCurrent(bool autoplay) {
    reportStopped();
    setError({});
    const Item track = m_queue.current();
    m_loadedId = track.id;
    if (track.id.isEmpty()) {
        m_media.stop();
        m_media.setSource({});
        updateArt();
        return;
    }
    m_playSessionId = QUuid::createUuid().toString(QUuid::Id128);
    m_reported = false;
    m_media.setSource(m_jellyfin->streamUrl(track.id));
    if (autoplay)
        m_media.play();
    updateArt();
    emit durationChanged();
    emit positionChanged();
}

void Player::advance(bool automatic) {
    const int next = m_queue.nextIndex(automatic);
    if (next < 0) {
        // The end of the queue: stop on its last track, ready to play again.
        reportStopped();
        m_media.stop();
        m_reported = false;
        return;
    }
    if (next == m_queue.currentIndex()) {
        reportStopped();
        m_playSessionId = QUuid::createUuid().toString(QUuid::Id128);
        m_reported = false;
        m_media.setPosition(0);
        m_media.play();
        return;
    }
    go(next, true);
}

void Player::play() {
    if (!hasTrack() && m_queue.count() > 0)
        go(0, true);
    else if (m_loadedId.isEmpty() && hasTrack())
        loadCurrent(true);
    else
        m_media.play();
}

void Player::pause() { m_media.pause(); }

void Player::playPause() {
    if (playing())
        pause();
    else
        play();
}

void Player::stop() {
    reportStopped();
    m_media.stop();
    m_reported = false;
}

void Player::next() {
    if (m_queue.count() == 0)
        return;
    const bool wasPlaying = playing() || m_buffering;
    const int next = m_queue.nextIndex(false);
    if (next >= 0)
        go(next, wasPlaying);
}

void Player::previous() {
    if (!hasTrack())
        return;
    if (m_media.position() > restartThresholdMs || m_queue.currentIndex() == m_queue.previousIndex()) {
        seek(0);
        return;
    }
    go(m_queue.previousIndex(), playing() || m_buffering);
}

void Player::seek(qint64 ms) {
    if (!hasTrack())
        return;
    ms = qBound<qint64>(0, ms, qMax<qint64>(0, duration() - 250));
    m_media.setPosition(ms);
    emit seeked(ms);
    reportProgress("TimeUpdate");
}

void Player::seekBy(qint64 ms) {
    seek(position() + ms);
}

void Player::clearError() { setError({}); }

void Player::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

void Player::updateArt() {
    const QUrl file = m_art.fetch(imageId(), imageTag(), QUrl(m_jellyfin->image(imageId(), imageTag(), 512)));
    if (file != m_artFile) {
        m_artFile = file;
        emit artFileChanged();
    }
}

QJsonObject Player::report() const {
    return {
        {"ItemId", m_loadedId},
        {"MediaSourceId", m_loadedId},
        {"PlaySessionId", m_playSessionId},
        {"PositionTicks", ticks(m_media.position())},
        {"IsPaused", m_media.playbackState() != QMediaPlayer::PlayingState},
        {"IsMuted", m_output.isMuted()},
        {"VolumeLevel", qRound(m_volume * 100)},
        {"CanSeek", true},
        {"PlayMethod", "DirectPlay"},
        {"RepeatMode", repeat() == Queue::RepeatOne ? "RepeatOne" : repeat() == Queue::RepeatAll ? "RepeatAll" : "RepeatNone"},
    };
}

void Player::reportStart() {
    if (m_loadedId.isEmpty())
        return;
    m_reported = true;
    m_jellyfin->post("/Sessions/Playing", report());
}

void Player::reportProgress(const QString &event) {
    if (!m_reported)
        return;
    QJsonObject body = report();
    body.insert("EventName", event);
    m_jellyfin->post("/Sessions/Playing/Progress", body);
}

void Player::reportStopped() {
    if (!m_reported)
        return;
    m_reported = false;
    m_progressTimer.stop();
    m_jellyfin->post("/Sessions/Playing/Stopped", report());
}
