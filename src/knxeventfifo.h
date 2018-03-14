#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "knxobject.h"

class KnxEventFifo
{
    bool _abort;
private:
    std::queue<KnxDataChanged> _queue;
    std::mutex _mutex;
    std::condition_variable _cond;

public:
    KnxEventFifo();
    ~KnxEventFifo();
    KnxDataChanged pop();
    void pop(KnxDataChanged& item);

    void push(const KnxDataChanged& item);
    void push(KnxDataChanged&& item);
    bool dataAvailable();
};
