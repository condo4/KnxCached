#pragma once

#include <vector>
#include <thread>

class KnxObjectPool;
class KnxScpiClient;

class KnxScpiServer
{
    KnxObjectPool *_pool;
    std::vector<KnxScpiClient *> _clients;
    bool _shutdown;
    std::thread _threadServer;

public:
    explicit KnxScpiServer(KnxObjectPool *pool);
    virtual ~KnxScpiServer();

    bool shutdown() const;
    void start();
    void stop();
    void join();
    void checkClients();

    KnxScpiClient *createClient(int serverFd);
    KnxObjectPool *pool() const;
};
