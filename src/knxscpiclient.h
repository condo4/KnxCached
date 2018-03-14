#pragma once

#include <thread>
#include <list>

#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write

#include "knxeventfifo.h"

class KnxScpiServer;
class KnxObject;

class KnxScpiClient
{
private:
    enum Mode {
        TextMode,
        DateTextMode,
        B64Mode
    };
    KnxScpiServer *_server;
    bool _shutdown;
    int _clientFd;
    struct sockaddr_in _client;
    std::thread _thread;
    bool _closed;
    std::list<const KnxObject *> _listen;
    KnxEventFifo _events;
    int _mode;

public:
    KnxScpiClient(KnxScpiServer* server, int srvfd);
    bool shutdown() const;

    void stop();
    void join();
    void closeFd();
    bool closed() const;
    KnxScpiServer *server() const;
};
