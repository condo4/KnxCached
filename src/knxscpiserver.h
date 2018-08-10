#pragma once

#include <vector>
#include <thread>
#include "knxscpiclient.h"
#include "knxobjectpool.h"


class KnxScpiServer
{
    KnxObjectPool &_pool;
    std::vector<KnxScpiClientPtr> _clients;
    bool _shutdown;
    std::thread _threadServer;

public:
    explicit KnxScpiServer(KnxObjectPool &pool);
    virtual ~KnxScpiServer();

    bool shutdown() const;
    void start();
    void stop();
    void join();
    void checkClients();

    KnxScpiClientPtr createClient(int serverFd);
    KnxObjectPool &pool() const;
};

typedef std::shared_ptr<KnxScpiServer> KnxScpiServerPtr;
