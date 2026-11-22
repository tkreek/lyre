#pragma once

#include <QList>
#include <QObject>

#include "itemmodel.h"

// The play queue: tracks in play order, which one is current, and shuffle
// and repeat. It plays nothing itself, so it can be tested without audio.
// Shuffling reorders the queue the view shows, and keeps the order it had
// so turning shuffle off puts it back.
class Queue : public QObject {
    Q_OBJECT

public:
    enum Repeat { RepeatOff, RepeatAll, RepeatOne };

    explicit Queue(QObject *parent = nullptr);

    ItemModel *model() { return &m_model; }
    int count() const { return m_model.count(); }
    int currentIndex() const { return m_current; }
    Item current() const { return m_model.at(m_current); }
    bool shuffle() const { return m_shuffle; }
    Repeat repeat() const { return m_repeat; }

    void replace(const QList<Item> &items, int start);
    void append(const QList<Item> &items);
    void insertNext(const QList<Item> &items);
    void remove(int row);
    void move(int from, int to);
    void clear();
    void setCurrent(int row);
    void setShuffle(bool shuffle);
    void setRepeat(Repeat repeat);

    // Where playback goes from here, or -1 to stop. `automatic` is true when
    // a track ran out on its own, which is the only time Repeat One repeats.
    int nextIndex(bool automatic) const;
    int previousIndex() const;

signals:
    void currentChanged();
    void shuffleChanged();
    void repeatChanged();

private:
    void shuffleAfterCurrent();

    ItemModel m_model;
    QList<quint64> m_keys;     // one per row, in play order
    QList<quint64> m_original; // the same keys in the unshuffled order
    quint64 m_nextKey = 1;
    int m_current = -1;
    bool m_shuffle = false;
    Repeat m_repeat = RepeatOff;
};
