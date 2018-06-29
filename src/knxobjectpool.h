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
    static KnxObjectPool* _instance;
    std::map<unsigned short, KnxObject*> _pool;
    EIBConnection *_connection;
    bool _shutdown;
    std::thread _thread;
    fd_set _read;

public:
    KnxObjectPool(std::string conffile);
    virtual ~KnxObjectPool();

    static KnxObjectPool *instance();

    unsigned char type(unsigned short addr) const;
    void addEventForAll(KnxEventFifo *ev) const;
    void write(unsigned short src, unsigned short dest, const unsigned char *buf, int len);
    bool shutdown() const;
    void stop();
    void start();
    void join();
    int send(unsigned short dest, std::vector<unsigned char> data);

    int getObjIds(std::vector<const char *> &param) const;
    KnxObject *getObjById(const char *id);
    KnxObject *getObjById(const std::string& id);
    const KnxObject *getObjByGad(unsigned short addr) const;
};
