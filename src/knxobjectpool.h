#pragma once

#include <string>
#include <map>
#include <thread>
#include <memory>
#include <eibclient.h>

#include <nng/nng.h>

#include "knxobject.h"

class KnxObjectPool
{
private:
    std::map<unsigned short, KnxObjectPtr> _pool;
    EIBConnection *_connection;
    bool _shutdown;
    std::thread _thread;
    std::thread _responder;
    fd_set _read;
    nng_socket _pub;
    nng_socket _rep;

    int _send_string(nng_socket &s, std::string str);

public:
    explicit KnxObjectPool(std::string conffile);
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
    int publish(unsigned short gad, const KnxDataChanged &data);

    int getObjIds(std::vector<std::string> &param) const;
    KnxObjectPtr getObjById(const std::string &id);
    const KnxObjectPtr getObjByGad(unsigned short addr) const;
};

