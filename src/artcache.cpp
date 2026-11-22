#include "artcache.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

ArtCache::ArtCache(QNetworkAccessManager *network, const QString &directory, QObject *parent)
    : QObject(parent), m_network(network),
      m_directory(directory.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/lyre/art"
                      : directory) {}

QString ArtCache::pathFor(const QString &id, const QString &tag) const {
    // Ids and tags are hex from the server, but they still name a file, so
    // nothing else gets through.
    static const QRegularExpression unsafe("[^A-Za-z0-9]");
    return m_directory + "/" + QString(id).remove(unsafe) + "-" + QString(tag).remove(unsafe) + ".jpg";
}

QUrl ArtCache::fetch(const QString &id, const QString &tag, const QUrl &remote) {
    if (id.isEmpty() || tag.isEmpty() || !remote.isValid())
        return {};
    const QString path = pathFor(id, tag);
    if (QFileInfo::exists(path))
        return QUrl::fromLocalFile(path);
    if (m_pending.contains(path))
        return {};
    m_pending.insert(path, true);

    QNetworkReply *reply = m_network->get(QNetworkRequest(remote));
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        reply->deleteLater();
        m_pending.remove(path);
        if (reply->error() != QNetworkReply::NoError)
            return;
        QDir().mkpath(m_directory);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(reply->readAll()) < 0 || !file.commit())
            return;
        emit ready(id, tag, QUrl::fromLocalFile(path));
    });
    return {};
}
