#include "itemmodel.h"

ItemModel::ItemModel(QObject *parent) : QAbstractListModel(parent) {}

int ItemModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ItemModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &item = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return item.name;
    case IdRole: return item.id;
    case KindRole: return int(item.kind);
    case ArtistRole: return item.artistLine();
    case AlbumRole: return item.album;
    case AlbumIdRole: return item.albumId;
    case YearRole: return item.year;
    case IndexRole: return item.index;
    case DiscRole: return item.disc;
    case ChildCountRole: return item.childCount;
    case DurationRole: return item.durationMs;
    case ImageIdRole: return item.imageId;
    case ImageTagRole: return item.imageTag;
    }
    return {};
}

QHash<int, QByteArray> ItemModel::roleNames() const {
    return {
        {IdRole, "itemId"},       {KindRole, "kind"},         {NameRole, "name"},
        {ArtistRole, "artist"},   {AlbumRole, "album"},       {AlbumIdRole, "albumId"},
        {YearRole, "year"},       {IndexRole, "trackNumber"}, {DiscRole, "disc"},
        {ChildCountRole, "childCount"}, {DurationRole, "duration"},
        {ImageIdRole, "imageId"}, {ImageTagRole, "imageTag"},
    };
}

qint64 ItemModel::totalDurationMs() const {
    qint64 total = 0;
    for (const auto &item : m_items)
        total += item.durationMs;
    return total;
}

QVariantMap ItemModel::get(int row) const {
    QVariantMap map;
    if (row < 0 || row >= m_items.size())
        return map;
    const QModelIndex at = index(row);
    const auto roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(QString::fromUtf8(it.value()), data(at, it.key()));
    return map;
}

int ItemModel::indexOf(const QString &id) const {
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).id == id)
            return i;
    return -1;
}

bool ItemModel::multiDisc() const {
    for (const auto &item : m_items)
        if (item.disc > 1)
            return true;
    return false;
}

void ItemModel::setItems(const QList<Item> &items) {
    beginResetModel();
    m_items = items;
    endResetModel();
    emit countChanged();
}

void ItemModel::insert(int row, const QList<Item> &items) {
    if (items.isEmpty())
        return;
    row = qBound(0, row, m_items.size());
    beginInsertRows({}, row, row + items.size() - 1);
    for (int i = 0; i < items.size(); ++i)
        m_items.insert(row + i, items.at(i));
    endInsertRows();
    emit countChanged();
}

void ItemModel::remove(int row, int count) {
    if (row < 0 || count <= 0 || row + count > m_items.size())
        return;
    beginRemoveRows({}, row, row + count - 1);
    m_items.remove(row, count);
    endRemoveRows();
    emit countChanged();
}

void ItemModel::move(int from, int to) {
    if (from == to || from < 0 || to < 0 || from >= m_items.size() || to >= m_items.size())
        return;
    // Qt's destination row is the position before the move, one past `to`
    // when moving down.
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_items.move(from, to);
    endMoveRows();
}

void ItemModel::setLoading(bool loading) {
    if (loading == m_loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void ItemModel::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}
