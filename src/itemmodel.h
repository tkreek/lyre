#pragma once

#include <QAbstractListModel>
#include <QList>

#include "item.h"

// A list of library items for a QML view, filled asynchronously. Views bind
// to `loading` and `error` so a slow or failed request is shown, not hidden.
class ItemModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(qint64 totalDurationMs READ totalDurationMs NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        KindRole,
        NameRole,
        ArtistRole,
        AlbumRole,
        AlbumIdRole,
        YearRole,
        IndexRole,
        DiscRole,
        ChildCountRole,
        DurationRole,
        ImageIdRole,
        ImageTagRole,
        MembersRole,
        DiscCountRole,
    };
    Q_ENUM(Role)

    explicit ItemModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_items.size(); }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    qint64 totalDurationMs() const;

    const QList<Item> &items() const { return m_items; }
    Item at(int row) const { return m_items.value(row); }
    Q_INVOKABLE int indexOf(const QString &id) const;
    // One row as a plain object, keyed like the roles, for QML actions.
    Q_INVOKABLE QVariantMap get(int row) const;
    // Whether this album or playlist's tracks span more than one disc, so
    // the track list knows to show disc numbers.
    Q_INVOKABLE bool multiDisc() const;

    void setItems(const QList<Item> &items);
    void insert(int row, const QList<Item> &items);
    void remove(int row, int count = 1);
    void move(int from, int to);
    void setLoading(bool loading);
    void setError(const QString &error);

signals:
    void countChanged();
    void loadingChanged();
    void errorChanged();
    // Emitted once a request has filled the model, or failed.
    void loaded();

private:
    QList<Item> m_items;
    bool m_loading = false;
    QString m_error;
};
