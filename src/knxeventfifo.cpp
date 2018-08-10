#include "knxeventfifo.h"

KnxEventFifo::KnxEventFifo():
    _abort(false)
{

}

KnxEventFifo::~KnxEventFifo()
{
    _abort = true;
    _cond.notify_all();
}

KnxDataChanged KnxEventFifo::pop()
{
    std::unique_lock<std::mutex> mlock(_mutex);
    while (_queue.empty())
    {
        _cond.wait(mlock);
    }
    if(_abort) return KnxDataChanged();
    auto item = _queue.front();
    _queue.pop();
    return item;
}

void KnxEventFifo::pop(KnxDataChanged& item)
{
    std::unique_lock<std::mutex> mlock(_mutex);
    while (_queue.empty())
    {
        _cond.wait(mlock);
    }
    if(_abort) return;
    item = _queue.front();
    _queue.pop();
}

void KnxEventFifo::push(const KnxDataChanged& item)
{
    std::unique_lock<std::mutex> mlock(_mutex);
    _queue.push(item);
    mlock.unlock();
    _cond.notify_one();
}

void KnxEventFifo::push(KnxDataChanged&& item)
{
    std::unique_lock<std::mutex> mlock(_mutex);
    _queue.push(std::move(item));
    mlock.unlock();
    _cond.notify_one();
}

bool KnxEventFifo::dataAvailable() const
{
    return !_queue.empty();
}
