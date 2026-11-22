#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>
#include <functional>

// The connection to one Jellyfin server as one user. It signs in with a
// password once and keeps only the access token the server hands back, in
// ~/.config/lyre/session.json (mode 0600); the password is never stored.
class Jellyfin : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY sessionChanged)
    Q_PROPERTY(QString server READ server NOTIFY sessionChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY sessionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    using Callback = std::function<void(const QJsonDocument &json, const QString &error)>;

    struct Session {
        QString server, userId, userName, token, deviceId;
    };

    explicit Jellyfin(const QString &sessionPath = {}, QObject *parent = nullptr);

    bool signedIn() const { return !m_session.token.isEmpty(); }
    QString server() const { return m_session.server; }
    QString userName() const { return m_session.userName; }
    QString userId() const { return m_session.userId; }
    QString deviceId() const { return m_session.deviceId; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QNetworkAccessManager *network() { return &m_network; }

    Q_INVOKABLE void signIn(const QString &server, const QString &userName, const QString &password);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void clearError();
    // A server-side resized Primary image. Jellyfin serves these without a
    // token and marks them immutable when tagged, so they cache forever.
    Q_INVOKABLE QString image(const QString &id, const QString &tag, int size) const;
    // A direct stream of the original file; Qt's FFmpeg backend decodes
    // everything Jellyfin stores, so the server never has to transcode.
    QUrl streamUrl(const QString &id) const;

    // GET a JSON endpoint. `done` runs on the context's thread and is dropped
    // if the context is destroyed before the reply arrives.
    void get(const QString &path, const QUrlQuery &query, QObject *context, Callback done);
    // POST a JSON body and ignore the answer, for playback reports.
    void post(const QString &path, const QJsonObject &body);

    // Asks the server whether the saved token still works, and signs out if
    // it doesn't. An unreachable server leaves the session as it is.
    void verify();

    static QString normalizeServer(const QString &input);
    static Session loadSession(const QString &path);
    static bool saveSession(const QString &path, const Session &session);

signals:
    void sessionChanged();
    void busyChanged();
    void errorChanged();

private:
    QNetworkRequest request(const QUrl &url) const;
    QString describe(QNetworkReply *reply) const;
    void setBusy(bool busy);
    void setError(const QString &error);
    void expire();

    QString m_sessionPath;
    Session m_session;
    QNetworkAccessManager m_network;
    bool m_busy = false;
    QString m_error;
};
