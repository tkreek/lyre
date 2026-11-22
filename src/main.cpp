// lyre — a keyboard-first Jellyfin music player for Omarchy.

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include "jellyfin.h"
#include "library.h"
#include "mpris.h"
#include "player.h"
#include "theme.h"
#include "translator.h"

namespace {
// Covers come from the server once and then from disk: Jellyfin marks tagged
// images immutable, so the cache never has to ask again.
class CachingNetworkFactory : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager *create(QObject *parent) override {
        auto *network = new QNetworkAccessManager(parent);
        auto *cache = new QNetworkDiskCache(network);
        cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
                                 "/lyre/images");
        cache->setMaximumCacheSize(256 * 1024 * 1024);
        network->setCache(cache);
        return network;
    }
};
} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("lyre");
    app.setApplicationVersion("0.1.0");

    // Matches the window to lyre.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("lyre");
    app.setWindowIcon(QIcon::fromTheme("lyre"));

    // Launching lyre while it's running brings the running one forward
    // instead of starting a second player.
    if (Mpris::raiseRunningInstance())
        return 0;

    // FFmpeg's stream summary prints the stream URL, and with it the access
    // token, into the journal. A QT_LOGGING_RULES set for debugging still wins.
    QLoggingCategory::setFilterRules("qt.multimedia.ffmpeg*=false");

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Jellyfin jellyfin;
    Library library(&jellyfin);
    Player player(&jellyfin, &library);
    Mpris mpris(&player);
    QObject::connect(&mpris, &Mpris::quitRequested, &app, &QGuiApplication::quit);

    QSettings settings(QDir::homePath() + "/.config/lyre/lyre.conf", QSettings::IniFormat);
    player.setVolume(settings.value("volume", 1.0).toDouble());
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&] {
        settings.setValue("volume", player.volume());
        player.stop();
    });

    CachingNetworkFactory networkFactory;
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&networkFactory);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("jellyfin", &jellyfin);
    engine.rootContext()->setContextProperty("library", &library);
    engine.rootContext()->setContextProperty("player", &player);
    engine.rootContext()->setContextProperty("mpris", &mpris);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (jellyfin.signedIn())
        jellyfin.verify();
    return app.exec();
}
