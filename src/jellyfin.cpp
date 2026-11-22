#include "jellyfin.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QNetworkReply>
#include <QSaveFile>
#include <QUuid>

namespace {
constexpr int timeoutMs = 15000;
}

Jellyfin::Jellyfin(const QString &sessionPath, QObject *parent)
    : QObject(parent),
      m_sessionPath(sessionPath.isEmpty() ? QDir::homePath() + "/.config/lyre/session.json" : sessionPath) {
    m_network.setTransferTimeout(timeoutMs);
    m_session = loadSession(m_sessionPath);
    // The device id identifies this install to the server across sign-ins, so
    // the dashboard shows one device rather than a new one per login.
    if (m_session.deviceId.isEmpty()) {
        m_session.deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        saveSession(m_sessionPath, m_session);
    }
}

QString Jellyfin::normalizeServer(const QString &input) {
    QString text = input.trimmed();
    if (text.isEmpty())
        return {};
    if (!text.contains("://"))
        text.prepend("http://");
    QUrl url(text, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https"))
        return {};
    QString path = url.path();
    while (path.endsWith('/'))
        path.chop(1);
    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url.toString();
}

// The server's base address, from the URL the public-info probe finally
// answered at after any redirects.
QString Jellyfin::serverFromProbe(const QUrl &finalUrl, const QString &fallback) {
    const QString suffix = "/System/Info/Public";
    QString url = finalUrl.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment).toString();
    if (!url.endsWith(suffix))
        return fallback;
    url.chop(suffix.size());
    const QString normalized = normalizeServer(url);
    return normalized.isEmpty() ? fallback : normalized;
}

Jellyfin::Session Jellyfin::loadSession(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    return {o.value("server").toString(), o.value("userId").toString(), o.value("userName").toString(),
            o.value("token").toString(), o.value("deviceId").toString()};
}

// Written beside the target and renamed into place, readable only by the
// user, because the token is as good as the password until it's revoked.
bool Jellyfin::saveSession(const QString &path, const Session &session) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QJsonObject o{{"server", session.server}, {"userId", session.userId}, {"userName", session.userName},
                        {"token", session.token},   {"deviceId", session.deviceId}};
    file.write(QJsonDocument(o).toJson());
    return file.commit();
}

QNetworkRequest Jellyfin::request(const QUrl &url) const {
    QNetworkRequest request(url);
    auto quoted = [](QString value) { return value.remove('"').remove(','); };
    QString auth = QString("MediaBrowser Client=\"Lyre\", Device=\"%1\", DeviceId=\"%2\", Version=\"%3\"")
                       .arg(quoted(QHostInfo::localHostName()), m_session.deviceId,
                            QCoreApplication::applicationVersion());
    if (!m_session.token.isEmpty())
        auth += QString(", Token=\"%1\"").arg(m_session.token);
    request.setRawHeader("Authorization", auth.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return request;
}

QString Jellyfin::describe(QNetworkReply *reply) const {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401)
        return tr("The server didn't accept the sign-in.");
    if (status >= 500)
        return tr("The server had a problem (HTTP %1).").arg(status);
    if (status >= 400)
        return tr("The server refused the request (HTTP %1).").arg(status);
    switch (reply->error()) {
    case QNetworkReply::OperationCanceledError:
    case QNetworkReply::TimeoutError: return tr("The server took too long to answer.");
    case QNetworkReply::HostNotFoundError: return tr("Couldn't find that server.");
    case QNetworkReply::ConnectionRefusedError: return tr("The server refused the connection.");
    case QNetworkReply::SslHandshakeFailedError: return tr("The server's certificate isn't trusted.");
    default: return tr("Couldn't reach the server: %1").arg(reply->errorString());
    }
}

void Jellyfin::signIn(const QString &serverInput, const QString &userName, const QString &password) {
    const QString server = normalizeServer(serverInput);
    if (server.isEmpty()) {
        setError(tr("Enter the server's address, like http://192.168.1.10:8096."));
        return;
    }
    if (userName.trimmed().isEmpty()) {
        setError(tr("Enter your user name."));
        return;
    }
    setError({});
    setBusy(true);
    m_session.server = server;
    m_session.token.clear();

    // Ask the public info endpoint first, so a wrong address is reported as
    // that rather than as a failed password.
    QNetworkReply *probe = m_network.get(request(QUrl(server + "/System/Info/Public")));
    connect(probe, &QNetworkReply::finished, this, [=, this] {
        probe->deleteLater();
        const QJsonObject info = QJsonDocument::fromJson(probe->readAll()).object();
        if (probe->error() != QNetworkReply::NoError) {
            setBusy(false);
            setError(describe(probe));
            return;
        }
        if (!info.contains("Id") || !info.contains("Version")) {
            setBusy(false);
            setError(tr("That address doesn't look like a Jellyfin server."));
            return;
        }

        // A reverse proxy often redirects http to https. GETs follow that,
        // but a redirected POST loses its body, so the sign-in would reach
        // the server without credentials. Use wherever the probe ended up.
        const QString base = serverFromProbe(probe->url(), server);
        m_session.server = base;

        const QJsonObject body{{"Username", userName.trimmed()}, {"Pw", password}};
        QNetworkReply *auth = m_network.post(request(QUrl(base + "/Users/AuthenticateByName")),
                                             QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(auth, &QNetworkReply::finished, this, [=, this] {
            auth->deleteLater();
            setBusy(false);
            const int status = auth->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 401) {
                setError(tr("Wrong user name or password."));
                return;
            }
            if (auth->error() != QNetworkReply::NoError) {
                setError(describe(auth));
                return;
            }
            const QJsonObject o = QJsonDocument::fromJson(auth->readAll()).object();
            const QString token = o.value("AccessToken").toString();
            const QJsonObject user = o.value("User").toObject();
            if (token.isEmpty()) {
                setError(tr("The server signed you in but sent no access token."));
                return;
            }
            m_session.token = token;
            m_session.userId = user.value("Id").toString();
            m_session.userName = user.value("Name").toString(userName.trimmed());
            if (!saveSession(m_sessionPath, m_session))
                setError(tr("Signed in, but couldn't save the session to %1.").arg(m_sessionPath));
            emit sessionChanged();
        });
    });
}

void Jellyfin::signOut() {
    if (!signedIn())
        return;
    // Revoke the token on the server too; the session is gone locally
    // whether or not that request makes it.
    m_network.post(request(QUrl(m_session.server + "/Sessions/Logout")), QByteArray());
    expire();
}

void Jellyfin::expire() {
    m_session.token.clear();
    m_session.userId.clear();
    saveSession(m_sessionPath, m_session);
    emit sessionChanged();
}

void Jellyfin::clearError() {
    setError({});
}

QString Jellyfin::image(const QString &id, const QString &tag, int size) const {
    if (id.isEmpty() || tag.isEmpty() || m_session.server.isEmpty())
        return {};
    return QString("%1/Items/%2/Images/Primary?fillWidth=%3&fillHeight=%3&quality=90&tag=%4")
        .arg(m_session.server, id, QString::number(size), tag);
}

QUrl Jellyfin::streamUrl(const QString &id) const {
    QUrl url(m_session.server + "/Audio/" + id + "/stream");
    QUrlQuery query;
    query.addQueryItem("static", "true");
    query.addQueryItem("deviceId", m_session.deviceId);
    // The media pipeline can't add headers, so the stream authenticates with
    // the token in the query, the way Jellyfin's own clients do.
    query.addQueryItem("api_key", m_session.token);
    url.setQuery(query);
    return url;
}

void Jellyfin::get(const QString &path, const QUrlQuery &query, QObject *context, Callback done) {
    if (!signedIn()) {
        done({}, tr("Not signed in."));
        return;
    }
    QUrl url(m_session.server + path);
    url.setQuery(query);
    QNetworkReply *reply = m_network.get(request(url));
    QPointer<QObject> guard(context);
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        reply->deleteLater();
        if (!guard)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401) {
            done({}, tr("Your session has expired. Sign in again."));
            setError(tr("Your session has expired. Sign in again."));
            expire();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            done({}, describe(reply));
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            done({}, tr("The server sent something that isn't JSON."));
            return;
        }
        done(json, {});
    });
}

void Jellyfin::post(const QString &path, const QJsonObject &body) {
    if (!signedIn())
        return;
    QNetworkReply *reply = m_network.post(request(QUrl(m_session.server + path)),
                                          QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void Jellyfin::verify() {
    get("/Users/Me", {}, this, [this](const QJsonDocument &json, const QString &) {
        const QString name = json.object().value("Name").toString();
        if (!name.isEmpty() && name != m_session.userName) {
            m_session.userName = name;
            saveSession(m_sessionPath, m_session);
            emit sessionChanged();
        }
    });
}

void Jellyfin::setBusy(bool busy) {
    if (busy == m_busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Jellyfin::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}
