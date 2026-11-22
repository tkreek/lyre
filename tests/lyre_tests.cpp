#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

#include "item.h"
#include "itemmodel.h"
#include "jellyfin.h"
#include "library.h"
#include "mpris.h"
#include "player.h"
#include "queue.h"
#include "theme.h"
#include "translator.h"

namespace {
QJsonObject track(const QString &id, int index = 0, int disc = 0) {
    return QJsonObject{{"Id", id}, {"Type", "Audio"}, {"Name", "Track " + id}, {"IndexNumber", index},
                       {"ParentIndexNumber", disc}, {"RunTimeTicks", 1800000000.0}};
}

QList<Item> tracks(int n) {
    QList<Item> items;
    for (int i = 0; i < n; ++i)
        items << Item::fromJson(track(QString::number(i)));
    return items;
}

QStringList ids(Queue &queue) {
    QStringList out;
    for (const auto &item : queue.model()->items())
        out << item.id;
    return out;
}
} // namespace

class LyreTests : public QObject {
    Q_OBJECT

private slots:
    // ---- items

    void trackParsesFromServerJson() {
        const QJsonObject o{
            {"Id", "t1"}, {"Type", "Audio"}, {"Name", "Overture"}, {"Artists", QJsonArray{"A", "B"}},
            {"AlbumArtist", "Orchestra"}, {"Album", "1812"}, {"AlbumId", "a1"}, {"IndexNumber", 3},
            {"ParentIndexNumber", 2}, {"RunTimeTicks", 45702000000.0}, {"ImageTags", QJsonObject{{"Primary", "own"}}},
            {"AlbumPrimaryImageTag", "albumtag"},
        };
        const Item item = Item::fromJson(o);
        QCOMPARE(item.kind, Item::Track);
        QCOMPARE(item.artistLine(), QString("A, B"));
        QCOMPARE(item.durationMs, qint64(4570200));
        QCOMPARE(item.index, 3);
        QCOMPARE(item.disc, 2);
        // A track shows its album's cover, not its own embedded one.
        QCOMPARE(item.imageId, QString("a1"));
        QCOMPARE(item.imageTag, QString("albumtag"));
    }

    void albumWithoutArtHasNoImage() {
        const Item item = Item::fromJson({{"Id", "a"}, {"Type", "MusicAlbum"}, {"Name", "X"},
                                          {"AlbumArtist", "Y"}, {"ImageTags", QJsonObject{}}});
        QCOMPARE(item.kind, Item::Album);
        QCOMPARE(item.artistLine(), QString("Y"));
        QVERIFY(item.imageTag.isEmpty());
    }

    void parseItemsKeepsOnlyMusic() {
        const QJsonArray array{track("1"), QJsonObject{{"Id", "m"}, {"Type", "Movie"}}, track("2")};
        QCOMPARE(Library::parseItems(QJsonDocument(QJsonObject{{"Items", array}})).size(), 2);
        QCOMPARE(Library::parseItems(QJsonDocument(array)).size(), 2);
    }

    void modelReportsRowsAndDiscs() {
        ItemModel model;
        model.setItems({Item::fromJson(track("1", 1, 1)), Item::fromJson(track("2", 1, 2))});
        QCOMPARE(model.count(), 2);
        QCOMPARE(model.totalDurationMs(), qint64(360000));
        QVERIFY(model.multiDisc());
        QCOMPARE(model.get(1).value("itemId").toString(), QString("2"));
        QCOMPARE(model.indexOf("2"), 1);
        QVERIFY(model.get(5).isEmpty());
    }

    // ---- sets of discs

    void discNumbersComeFromNamesAndFolders() {
        QString base;
        QCOMPARE(Item::discFromName("Carmen (Disc 2)", &base), 2);
        QCOMPARE(base, QString("Carmen"));
        QCOMPARE(Item::discFromName("Messiah [CD 1 of 2]"), 1);
        QCOMPARE(Item::discFromName("Live at Disco Inferno"), 0);
        QCOMPARE(Item::discFromName("Abbey Road", &base), 0);
        QCOMPARE(base, QString("Abbey Road"));
        QCOMPARE(Item::discFromFolder("Carmen CD 3"), 3);
        QCOMPARE(Item::discFromFolder("Bach - Brandenburg Concertos [CD2.2]"), 2);
        QCOMPARE(Item::discFromFolder("Six Suites for Unaccompanied Cello Disc 2"), 2);
        QCOMPARE(Item::discFromFolder("Debussy - Complete Works (4Cd)"), 0);
        QCOMPARE(Item::discFromFolder("Bach - Concertos for piano (2 CD)"), 0);
    }

    void discsOfASetBecomeOneAlbum() {
        auto album = [](const QString &id, const QString &name, const QString &artist, int tracks,
                        const QString &path = {}) {
            return Item::fromJson({{"Id", id}, {"Type", "MusicAlbum"}, {"Name", name}, {"AlbumArtist", artist},
                                   {"ChildCount", tracks}, {"Path", path}});
        };
        const QList<Item> albums{
            album("abbey", "Abbey Road", "The Beatles", 17),
            // Listed out of order, and disc 2 credits a different artist.
            album("b2", "Bach: Brandenburg Concertos (Disc 2)", "J.S. Bach - OAE", 9),
            album("b1", "Bach: Brandenburg Concertos (Disc 1)", "Bach", 10),
            album("c1", "Carmen (Disc 1)", "Bizet", 15),
            album("c2", "Carmen (Disc 2)", "Bizet", 13),
            album("c3", "Carmen (Disc 3)", "Bizet", 14),
            // Same name twice; only the folder says which disc.
            album("y2", "Cello Suites", "Bach", 18, "/m/Yo-Yo Ma/Six Suites Disc 2"),
            album("y1", "Cello Suites", "Bach", 18, "/m/Yo-Yo Ma/Bach- Six Suites"),
            // Two artists' first discs of differently-owned sets stay apart.
            album("g1", "Greatest Hits (Disc 1)", "Artist A", 10),
            album("h1", "Greatest Hits (Disc 1)", "Artist B", 11),
            // A lone disc stays as it is.
            album("j1", "Johannes Passion (Disc 1)", "Bach", 21),
            // Same name, different artists, no disc markers: not a set.
            album("m1", "Messiah", "Hogwood", 2),
            album("m2", "Messiah", "Pinnock", 3),
        };
        const QList<Item> grouped = Item::groupDiscSets(albums);
        QStringList names;
        for (const auto &item : grouped)
            names << item.name;
        QCOMPARE(names, (QStringList{"Abbey Road", "Bach: Brandenburg Concertos", "Carmen", "Cello Suites",
                                      "Greatest Hits (Disc 1)", "Greatest Hits (Disc 1)",
                                      "Johannes Passion (Disc 1)", "Messiah", "Messiah"}));
        QCOMPARE(grouped.at(1).members, (QStringList{"b1", "b2"}));
        QCOMPARE(grouped.at(2).members, (QStringList{"c1", "c2", "c3"}));
        QCOMPARE(grouped.at(2).childCount, 42);
        QCOMPARE(grouped.at(3).members, (QStringList{"y1", "y2"}));
        QVERIFY(grouped.at(0).members.isEmpty());
        QVERIFY(grouped.at(6).members.isEmpty());
    }

    void setTracksPlayDiscByDisc() {
        auto track = [](const QString &id, const QString &album, int index, int disc) {
            return Item::fromJson({{"Id", id}, {"Type", "Audio"}, {"AlbumId", album}, {"IndexNumber", index},
                                   {"ParentIndexNumber", disc}});
        };
        // The server sorted by the files' own disc tags, which all say 1.
        // (Each disc's tracks keep their own numbers.)
        const QList<Item> fetched{track("c2-1", "c2", 1, 1), track("c1-1", "c1", 1, 1), track("c3-1", "c3", 1, 1),
                                  track("c1-2", "c1", 2, 1), track("c2-2", "c2", 2, 1)};
        const QList<Item> ordered = Library::orderSetTracks({"c1", "c2", "c3"}, fetched);
        QStringList ids;
        QList<int> discs;
        for (const auto &item : ordered) {
            ids << item.id;
            discs << item.disc;
        }
        QCOMPARE(ids, (QStringList{"c1-1", "c1-2", "c2-1", "c2-2", "c3-1"}));
        QCOMPARE(discs, (QList<int>{1, 1, 2, 2, 3}));
    }

    void untaggedTracksOrderByFileName() {
        auto track = [](const QString &name, const QString &file) {
            return Item::fromJson({{"Id", name}, {"Type", "Audio"}, {"Name", name},
                                   {"Path", "/m/Carmen CD 3/" + file}});
        };
        const QList<Item> fetched{track("A deux cuartos", "11 - A deux cuartos.flac"),
                                  track("Ecoute", "02 - Ecoute.flac"),
                                  track("Entracto", "10 - Entracto.flac"),
                                  track("Bonus", "Bonus.flac")};
        QCOMPARE(fetched.at(0).index, 11);
        QCOMPARE(fetched.at(3).index, 0);
        QStringList order;
        for (const auto &item : Item::sortAlbumTracks(fetched))
            order << item.id;
        QCOMPARE(order, (QStringList{"Ecoute", "Entracto", "A deux cuartos", "Bonus"}));
    }

    // ---- server and session

    void serverAddressesNormalize() {
        QCOMPARE(Jellyfin::normalizeServer("192.168.1.10:8096"), QString("http://192.168.1.10:8096"));
        QCOMPARE(Jellyfin::normalizeServer(" https://jf.example.com/jellyfin/ "), QString("https://jf.example.com/jellyfin"));
        QCOMPARE(Jellyfin::normalizeServer("http://host:8096/?x=1#y"), QString("http://host:8096"));
        QVERIFY(Jellyfin::normalizeServer("").isEmpty());
        QVERIFY(Jellyfin::normalizeServer("ftp://host").isEmpty());
    }

    void redirectedProbeSetsTheServer() {
        // http://host typed, the proxy redirected to https.
        QCOMPARE(Jellyfin::serverFromProbe(QUrl("https://jf.example.com/System/Info/Public"), "http://jf.example.com"),
                 QString("https://jf.example.com"));
        QCOMPARE(Jellyfin::serverFromProbe(QUrl("https://example.com/jellyfin/System/Info/Public"), "http://x"),
                 QString("https://example.com/jellyfin"));
        QCOMPARE(Jellyfin::serverFromProbe(QUrl("https://example.com/elsewhere"), "http://x"), QString("http://x"));
    }

    void sessionIsPrivateAndRoundTrips() {
        QTemporaryDir dir;
        const QString path = dir.filePath("nested/session.json");
        QVERIFY(Jellyfin::saveSession(path, {"http://s", "u1", "me", "tok", "dev"}));
        QCOMPARE(QFileInfo(path).permissions() & (QFile::ReadGroup | QFile::ReadOther), QFileDevice::Permissions());
        const auto session = Jellyfin::loadSession(path);
        QCOMPARE(session.server, QString("http://s"));
        QCOMPARE(session.token, QString("tok"));
        QCOMPARE(session.deviceId, QString("dev"));
    }

    void newInstallGetsADeviceIdAndStaysSignedOut() {
        QTemporaryDir dir;
        Jellyfin jellyfin(dir.filePath("session.json"));
        QVERIFY(!jellyfin.signedIn());
        QVERIFY(!jellyfin.deviceId().isEmpty());
        Jellyfin again(dir.filePath("session.json"));
        QCOMPARE(again.deviceId(), jellyfin.deviceId());
    }

    void urlsCarryWhatTheServerNeeds() {
        QTemporaryDir dir;
        Jellyfin::saveSession(dir.filePath("s.json"), {"http://s:8096", "u", "me", "tok", "dev"});
        Jellyfin jellyfin(dir.filePath("s.json"));
        QVERIFY(jellyfin.signedIn());
        QCOMPARE(jellyfin.image("a1", "t", 240), QString("http://s:8096/Items/a1/Images/Primary?fillWidth=240&fillHeight=240&quality=90&tag=t"));
        QVERIFY(jellyfin.image("a1", "", 240).isEmpty());
        const QUrl stream = jellyfin.streamUrl("x");
        QCOMPARE(stream.path(), QString("/Audio/x/stream"));
        QCOMPARE(QUrlQuery(stream).queryItemValue("api_key"), QString("tok"));
        QCOMPARE(QUrlQuery(stream).queryItemValue("static"), QString("true"));
    }

    void signInRejectsABadAddressWithoutNetwork() {
        QTemporaryDir dir;
        Jellyfin jellyfin(dir.filePath("s.json"));
        jellyfin.signIn("", "me", "pw");
        QVERIFY(!jellyfin.error().isEmpty());
        QVERIFY(!jellyfin.busy());
    }

    // ---- queue

    void queueAdvancesAndStops() {
        Queue queue;
        queue.replace(tracks(3), 0);
        QCOMPARE(queue.nextIndex(false), 1);
        queue.setCurrent(2);
        QCOMPARE(queue.nextIndex(true), -1);
        QCOMPARE(queue.previousIndex(), 1);
    }

    void repeatModes() {
        Queue queue;
        queue.replace(tracks(3), 2);
        queue.setRepeat(Queue::RepeatAll);
        QCOMPARE(queue.nextIndex(true), 0);
        queue.setCurrent(0);
        QCOMPARE(queue.previousIndex(), 2);
        queue.setRepeat(Queue::RepeatOne);
        // Repeat One repeats only when the track ends by itself; Next skips.
        QCOMPARE(queue.nextIndex(true), 0);
        QCOMPARE(queue.nextIndex(false), 1);
    }

    void shuffleKeepsCurrentFirstAndRestores() {
        Queue queue;
        queue.replace(tracks(20), 7);
        queue.setShuffle(true);
        QCOMPARE(queue.currentIndex(), 0);
        QCOMPARE(queue.current().id, QString("7"));
        QCOMPARE(queue.count(), 20);
        auto shuffled = ids(queue);
        shuffled.sort();
        auto original = ids(queue);
        QCOMPARE(QSet<QString>(shuffled.begin(), shuffled.end()).size(), 20);

        queue.setShuffle(false);
        QStringList expected;
        for (int i = 0; i < 20; ++i)
            expected << QString::number(i);
        QCOMPARE(ids(queue), expected);
        QCOMPARE(queue.currentIndex(), 7);
        Q_UNUSED(original)
    }

    void playNextLandsAfterCurrentEvenAfterUnshuffle() {
        Queue queue;
        queue.replace(tracks(5), 1);
        queue.setShuffle(true);
        Item extra = Item::fromJson(track("x"));
        queue.insertNext({extra});
        QCOMPARE(queue.model()->at(queue.currentIndex() + 1).id, QString("x"));
        queue.setShuffle(false);
        QCOMPARE(ids(queue), (QStringList{"0", "1", "x", "2", "3", "4"}));
    }

    void removingAndMovingKeepCurrent() {
        Queue queue;
        queue.replace(tracks(4), 2);
        queue.remove(0);
        QCOMPARE(queue.current().id, QString("2"));
        queue.move(1, 0);
        QCOMPARE(queue.currentIndex(), 0);
        QCOMPARE(queue.current().id, QString("2"));
        QSignalSpy changed(&queue, &Queue::currentChanged);
        queue.remove(0);
        QCOMPARE(changed.count(), 1);
        // The track that followed the removed one in play order takes over.
        QCOMPARE(queue.current().id, QString("1"));
        queue.remove(1);
        queue.remove(0);
        QCOMPARE(queue.currentIndex(), -1);
    }

    void duplicatesAreSeparateEntries() {
        Queue queue;
        queue.replace(tracks(1), 0);
        queue.append(tracks(1));
        queue.setShuffle(true);
        queue.setShuffle(false);
        QCOMPARE(queue.count(), 2);
    }

    // ---- player and MPRIS, without the network

    void emptyPlayerPublishesNoTrack() {
        QTemporaryDir dir;
        Jellyfin jellyfin(dir.filePath("s.json"));
        Library library(&jellyfin);
        Player player(&jellyfin, &library);
        QVERIFY(!player.hasTrack());
        QCOMPARE(player.status(), QString("Stopped"));
        const auto metadata = Mpris::metadata(&player);
        QCOMPARE(metadata.value("mpris:trackid").value<QDBusObjectPath>().path(),
                 QString("/org/mpris/MediaPlayer2/TrackList/NoTrack"));
    }

    void playerVolumeIsClampedAndRepeatCycles() {
        QTemporaryDir dir;
        Jellyfin jellyfin(dir.filePath("s.json"));
        Library library(&jellyfin);
        Player player(&jellyfin, &library);
        player.setVolume(2.0);
        QCOMPARE(player.volume(), 1.0);
        player.setVolume(-1);
        QCOMPARE(player.volume(), 0.0);
        player.cycleRepeat();
        QCOMPARE(player.repeat(), 1);
        player.cycleRepeat();
        player.cycleRepeat();
        QCOMPARE(player.repeat(), 0);
    }

    // ---- theme

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QVERIFY(theme.dark());
        QCOMPARE(theme.fontSize(), 14);
    }

    void themeReadsThePalette() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "mode = \"light\"\naccent = \"#112233\"\nbackground = \"#fafafa\"\ndark_foreground = \"#777777\"\n");
        write(dir.path() + "/theme/shell.toml", "[bar]\nbase-size = 40\n[font]\n# base-size = 9\nbase-size = 18\n");
        Theme theme(dir.path());
        QVERIFY(!theme.dark());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
        QCOMPARE(theme.background(), QString("#fafafa"));
        // A theme without `muted` uses its dark_foreground.
        QCOMPARE(theme.muted(), QString("#777777"));
        // Missing keys fall back to the light palette in a light theme.
        QCOMPARE(theme.foreground(), QString("#1a1a1a"));
        QCOMPARE(theme.fontSize(), 21);
    }

    void themeFollowsChanges() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QFile::remove(dir.path() + "/theme/colors.toml");
        write(dir.path() + "/theme/colors.toml", "accent = \"#445566\"\n");
        QTRY_COMPARE(theme.accent(), QString("#445566"));
    }

    void themeFollowsHyprlandRounding() {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    // ---- translator

    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
    }

    void shippedCatalogsAreValid() {
        // Every catalog compiled into the app parses, and its plural entries
        // are objects with an "other" form.
        const QDir catalogs(QFINDTESTDATA("../src/i18n"));
        const auto files = catalogs.entryList({"*.json"});
        QVERIFY(files.contains("en.json"));
        for (const auto &name : files) {
            QFile file(catalogs.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError error;
            const auto json = QJsonDocument::fromJson(file.readAll(), &error);
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(name + ": " + error.errorString()));
            const auto object = json.object();
            for (auto it = object.begin(); it != object.end(); ++it)
                if (it.key().contains("%n"))
                    QVERIFY2(it.value().toObject().contains("other"), qPrintable(name + ": " + it.key()));
        }
    }

private:
    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(LyreTests)
#include "lyre_tests.moc"
