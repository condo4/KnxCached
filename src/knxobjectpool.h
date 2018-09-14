#pragma once

#include <string>
#include <map>
#include <thread>
#include <memory>
#include <eibclient.h>

#include "knxobject.h"

class KnxObjectPool
{
private:
    std::map<unsigned short, KnxObjectPtr> _pool;
    EIBConnection *_connection;
    bool _shutdown;
    std::thread _thread;
    fd_set _read;
    void *_pub;

public:
    explicit KnxObjectPool(void *context, std::string conffile);
    virtual ~KnxObjectPool();

    unsigned char type(unsigned short addr) const;
    void addEventForAll(KnxEventFifo *ev) const;
    void write(unsigned short src, unsigned short dest, const unsigned char *buf, int len);
    bool shutdown() const;
    void stop();
    void start();
    void join();
    int send(unsigned short dest, std::vector<unsigned char> data);
    int publish(std::string topic, std::string value);

    int getObjIds(std::vector<std::string> &param) const;
    KnxObjectPtr getObjById(const std::string &id);
    const KnxObjectPtr getObjByGad(unsigned short addr) const;
};

