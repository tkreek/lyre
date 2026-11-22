#include "mpris.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QRegularExpression>

#include "player.h"

namespace {
const QString objectPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString baseService = QStringLiteral("org.mpris.MediaPlayer2.lyre");
const QString playerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");

// MPRIS times are microseconds.
qlonglong micro(qint64 ms) { return ms * 1000; }

QDBusObjectPath trackPath(const QString &id) {
    if (id.isEmpty())
        return QDBusObjectPath("/org/mpris/MediaPlayer2/TrackList/NoTrack");
    QString safe = id;
    safe.remove(QRegularExpression("[^A-Za-z0-9_]"));
    return QDBusObjectPath("/org/lyre/track/t" + safe);
}
} // namespace

Mpris::Mpris(Player *player, QObject *parent) : QObject(parent), m_player(player) {
    new MprisRoot(this);
    auto *adaptor = new MprisPlayer(this);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.registerObject(objectPath, this))
        return;
    // A second running copy (rare, since launching lyre again raises the
    // first) gets its own instance name, as the spec allows.
    for (const QString &name : {baseService, baseService + ".instance" + QString::number(QCoreApplication::applicationPid())}) {
        if (bus.registerService(name)) {
            m_service = name;
            break;
        }
    }

    auto status = [this] {
        changed(playerInterface, {{"PlaybackStatus", m_player->status()},
                                  {"CanPlay", m_player->hasTrack()}, {"CanPause", m_player->hasTrack()},
                                  {"CanSeek", m_player->hasTrack()}});
    };
    connect(player, &Player::stateChanged, this, status);
    connect(player, &Player::currentChanged, this, [this] {
        changed(playerInterface, {{"Metadata", metadata(m_player)}, {"CanGoNext", m_player->canNext()},
                                  {"CanGoPrevious", m_player->hasTrack()}, {"CanPlay", m_player->hasTrack()},
                                  {"CanPause", m_player->hasTrack()}, {"CanSeek", m_player->hasTrack()}});
    });
    auto metadataChanged = [this] { changed(playerInterface, {{"Metadata", metadata(m_player)}}); };
    connect(player, &Player::artFileChanged, this, metadataChanged);
    connect(player, &Player::durationChanged, this, metadataChanged);
    connect(player, &Player::volumeChanged, this, [this] {
        changed(playerInterface, {{"Volume", m_player->muted() ? 0.0 : m_player->volume()}});
    });
    connect(player, &Player::shuffleChanged, this, [this, adaptor] {
        changed(playerInterface, {{"Shuffle", adaptor->shuffle()}});
    });
    connect(player, &Player::repeatChanged, this, [this, adaptor] {
        changed(playerInterface, {{"LoopStatus", adaptor->loopStatus()}, {"CanGoNext", m_player->canNext()}});
    });
    connect(player, &Player::seeked, adaptor, [adaptor](qint64 ms) { emit adaptor->Seeked(micro(ms)); });
}

Mpris::~Mpris() {
    if (registered())
        QDBusConnection::sessionBus().unregisterService(m_service);
}

bool Mpris::raiseRunningInstance() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()->isServiceRegistered(baseService).value())
        return false;
    const auto message = QDBusMessage::createMethodCall(baseService, objectPath, "org.mpris.MediaPlayer2", "Raise");
    return bus.call(message, QDBus::Block, 2000).type() == QDBusMessage::ReplyMessage;
}

QVariantMap Mpris::metadata(Player *player) {
    if (!player->hasTrack())
        return {{"mpris:trackid", QVariant::fromValue(trackPath({}))}};
    QVariantMap map{
        {"mpris:trackid", QVariant::fromValue(trackPath(player->currentId()))},
        {"mpris:length", micro(player->duration())},
        {"xesam:title", player->title()},
        {"xesam:artist", player->artists()},
        {"xesam:album", player->album()},
    };
    if (!player->artFile().isEmpty())
        map.insert("mpris:artUrl", player->artFile().toString());
    return map;
}

void Mpris::changed(const QString &interface, const QVariantMap &properties) {
    if (!registered())
        return;
    QDBusMessage signal = QDBusMessage::createSignal(objectPath, "org.freedesktop.DBus.Properties", "PropertiesChanged");
    signal << interface << properties << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

MprisRoot::MprisRoot(Mpris *mpris) : QDBusAbstractAdaptor(mpris), m_mpris(mpris) {}
void MprisRoot::Raise() { emit m_mpris->raiseRequested(); }
void MprisRoot::Quit() { emit m_mpris->quitRequested(); }

MprisPlayer::MprisPlayer(Mpris *mpris) : QDBusAbstractAdaptor(mpris), m_player(mpris->player()) {}

QString MprisPlayer::playbackStatus() const { return m_player->status(); }

QString MprisPlayer::loopStatus() const {
    switch (m_player->repeat()) {
    case 1: return "Playlist";
    case 2: return "Track";
    default: return "None";
    }
}

void MprisPlayer::setLoopStatus(const QString &status) {
    m_player->setRepeat(status == "Playlist" ? 1 : status == "Track" ? 2 : 0);
}

bool MprisPlayer::shuffle() const { return m_player->shuffle(); }
void MprisPlayer::setShuffle(bool shuffle) { m_player->setShuffle(shuffle); }
QVariantMap MprisPlayer::metadata() const { return Mpris::metadata(m_player); }
double MprisPlayer::volume() const { return m_player->muted() ? 0.0 : m_player->volume(); }

void MprisPlayer::setVolume(double volume) {
    m_player->setMuted(false);
    m_player->setVolume(volume);
}

qlonglong MprisPlayer::position() const { return micro(m_player->position()); }
bool MprisPlayer::canGoNext() const { return m_player->canNext(); }
bool MprisPlayer::canGoPrevious() const { return m_player->hasTrack(); }
bool MprisPlayer::canPlay() const { return m_player->hasTrack(); }

void MprisPlayer::Next() { m_player->next(); }
void MprisPlayer::Previous() { m_player->previous(); }
void MprisPlayer::Pause() { m_player->pause(); }
void MprisPlayer::PlayPause() { m_player->playPause(); }
void MprisPlayer::Stop() { m_player->stop(); }
void MprisPlayer::Play() { m_player->play(); }
void MprisPlayer::Seek(qlonglong offset) { m_player->seekBy(offset / 1000); }

void MprisPlayer::SetPosition(const QDBusObjectPath &trackId, qlonglong position) {
    // The spec ignores a SetPosition for a track that's no longer current.
    if (trackId == trackPath(m_player->currentId()))
        m_player->seek(position / 1000);
}
