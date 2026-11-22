#include "queue.h"

#include <QHash>
#include <QRandomGenerator>
#include <algorithm>

Queue::Queue(QObject *parent) : QObject(parent) {}

void Queue::replace(const QList<Item> &items, int start) {
    m_keys.clear();
    for (int i = 0; i < items.size(); ++i)
        m_keys << m_nextKey++;
    m_original = m_keys;
    m_model.setItems(items);
    m_current = items.isEmpty() ? -1 : qBound(0, start, int(items.size()) - 1);
    if (m_shuffle)
        shuffleAfterCurrent();
    emit currentChanged();
}

void Queue::append(const QList<Item> &items) {
    QList<quint64> keys;
    for (int i = 0; i < items.size(); ++i)
        keys << m_nextKey++;
    m_keys += keys;
    m_original += keys;
    m_model.insert(m_model.count(), items);
}

void Queue::insertNext(const QList<Item> &items) {
    if (m_current < 0) {
        append(items);
        return;
    }
    QList<quint64> keys;
    for (int i = 0; i < items.size(); ++i)
        keys << m_nextKey++;
    const int at = m_current + 1;
    for (int i = 0; i < keys.size(); ++i)
        m_keys.insert(at + i, keys.at(i));
    // In the unshuffled order they also follow the current track, so turning
    // shuffle off doesn't send them to the end.
    const int originalAt = int(m_original.indexOf(m_keys.at(m_current))) + 1;
    for (int i = 0; i < keys.size(); ++i)
        m_original.insert(originalAt + i, keys.at(i));
    m_model.insert(at, items);
}

void Queue::remove(int row) {
    if (row < 0 || row >= m_keys.size())
        return;
    m_original.removeOne(m_keys.at(row));
    m_keys.removeAt(row);
    m_model.remove(row);
    if (row < m_current) {
        --m_current;
    } else if (row == m_current) {
        // The track after the removed one becomes current, as if skipped to.
        if (m_current >= m_keys.size())
            m_current = m_keys.size() - 1;
        emit currentChanged();
    }
}

void Queue::move(int from, int to) {
    if (from == to || from < 0 || to < 0 || from >= m_keys.size() || to >= m_keys.size())
        return;
    m_keys.move(from, to);
    m_model.move(from, to);
    if (from == m_current)
        m_current = to;
    else if (from < m_current && to >= m_current)
        --m_current;
    else if (from > m_current && to <= m_current)
        ++m_current;
}

void Queue::clear() {
    replace({}, -1);
}

void Queue::setCurrent(int row) {
    if (row < -1 || row >= m_keys.size() || row == m_current)
        return;
    m_current = row;
    emit currentChanged();
}

void Queue::setShuffle(bool shuffle) {
    if (shuffle == m_shuffle)
        return;
    m_shuffle = shuffle;
    if (shuffle) {
        shuffleAfterCurrent();
    } else if (!m_keys.isEmpty()) {
        // Put the original order back, keeping whichever track is playing.
        const quint64 currentKey = m_current >= 0 ? m_keys.at(m_current) : 0;
        QHash<quint64, Item> byKey;
        for (int i = 0; i < m_keys.size(); ++i)
            byKey.insert(m_keys.at(i), m_model.at(i));
        QList<Item> items;
        for (quint64 key : std::as_const(m_original))
            items << byKey.value(key);
        m_keys = m_original;
        m_model.setItems(items);
        m_current = currentKey ? int(m_keys.indexOf(currentKey)) : -1;
        emit currentChanged();
    }
    emit shuffleChanged();
}

// The current track moves to the top and everything else follows in random
// order, so the whole queue is ahead of it, as in every other player.
void Queue::shuffleAfterCurrent() {
    if (m_keys.size() < 2)
        return;
    QList<int> rows;
    for (int i = 0; i < m_keys.size(); ++i)
        if (i != m_current)
            rows << i;
    std::shuffle(rows.begin(), rows.end(), *QRandomGenerator::global());
    if (m_current >= 0)
        rows.prepend(m_current);

    QList<quint64> keys;
    QList<Item> items;
    for (int row : std::as_const(rows)) {
        keys << m_keys.at(row);
        items << m_model.at(row);
    }
    m_keys = keys;
    m_model.setItems(items);
    if (m_current >= 0) {
        m_current = 0;
        emit currentChanged();
    }
}

void Queue::setRepeat(Repeat repeat) {
    if (repeat == m_repeat)
        return;
    m_repeat = repeat;
    emit repeatChanged();
}

int Queue::nextIndex(bool automatic) const {
    if (m_keys.isEmpty())
        return -1;
    if (automatic && m_repeat == RepeatOne)
        return m_current;
    if (m_current + 1 < m_keys.size())
        return m_current + 1;
    return m_repeat == RepeatOff ? -1 : 0;
}

int Queue::previousIndex() const {
    if (m_keys.isEmpty())
        return -1;
    if (m_current > 0)
        return m_current - 1;
    return m_repeat == RepeatAll ? m_keys.size() - 1 : 0;
}
