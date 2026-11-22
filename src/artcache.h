#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

// Album covers saved as files, for the parts of the desktop that want a
// local path: the Omarchy shell's media panel and notifications read
// MPRIS's artUrl, and a file:// URL works there even when the server is far
// away or slow.
class ArtCache : public QObject {
    Q_OBJECT

public:
    ArtCache(QNetworkAccessManager *network, const QString &directory = {}, QObject *parent = nullptr);

    QString pathFor(const QString &id, const QString &tag) const;
    // The cover as a file URL if it's already saved; otherwise starts the
    // download, emits ready() when it lands, and returns an empty URL.
    QUrl fetch(const QString &id, const QString &tag, const QUrl &remote);

signals:
    void ready(const QString &id, const QString &tag, const QUrl &file);

private:
    QNetworkAccessManager *m_network;
    QString m_directory;
    QHash<QString, bool> m_pending;
};
