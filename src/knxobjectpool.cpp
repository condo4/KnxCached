#include "knxobjectpool.h"

#include <sstream>
#include <iostream>
#include <string>
#include <thread>
#include <cassert>
#include <cstring>

#include <libxml/encoding.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/reqrep0/rep.h>

#include "eibclient.h"

using namespace std;

KnxObjectPool::KnxObjectPool(string conffile)
    : _connection(nullptr)
    , _shutdown(false)
    , _read({0,})
{
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlDocPtr doc;

    xmlInitParser();

    doc = xmlParseFile(conffile.c_str());
    if (doc == nullptr)
    {
        cerr << "Error: unable to parse file " << conffile << endl;
        exit(1);
    }

    xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == nullptr)
    {
        cerr << "Error: unable to create new XPath context" << endl;
        exit(1);
    }

    /* Connect to knxd */
    xpathObj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//knxd"), xpathCtx);
    if(xpathObj == nullptr) {
        cerr << "Error: unable to evaluate xpath expression" << endl;
        xmlXPathFreeContext(xpathCtx);
        exit(1);
    }
    if(xpathObj->nodesetval->nodeNr != 1) {
        cerr << "Error: unable to evaluate xpath expression" << endl;
        xmlXPathFreeContext(xpathCtx);
        exit(1);
    }
    xmlNodePtr knxdnode = xpathObj->nodesetval->nodeTab[0];
    xmlChar *url = xmlGetProp(knxdnode, reinterpret_cast<const xmlChar*>("url"));
    _connection = EIBSocketURL (reinterpret_cast<char*>(url));
    if (EIBOpen_GroupSocket (_connection, 0) == -1){
        std::cout << "Error opening knxd socket (" << reinterpret_cast<char*>(url) << ")" << std::endl;
        xmlXPathFreeContext(xpathCtx);
        exit(1);
    }
    xmlFree(url);
    xmlXPathFreeObject(xpathObj);

    /* Create all objects */
    xpathObj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//object"), xpathCtx);
    if(xpathObj == nullptr) {
        cerr << "Error: unable to evaluate xpath expression" << endl;
        xmlXPathFreeContext(xpathCtx);
        exit(1);
    }

    for(int i = 0; i < xpathObj->nodesetval->nodeNr; i++)
    {
        xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
        xmlChar *id = xmlGetProp(node, reinterpret_cast<const xmlChar*>("id"));
        xmlChar *type = xmlGetProp(node, reinterpret_cast<const xmlChar*>("type"));
        xmlChar *gad = xmlGetProp(node, reinterpret_cast<const xmlChar*>("gad"));
        unsigned short rgad = StringToGroupAddress(reinterpret_cast<char*>(gad));
        if(_pool.find(rgad) != _pool.end())
        {
            cerr << "Error: There is 2 object with " << GroupAddressToString(rgad) << " key" << endl;
            xmlFree(id);
            xmlFree(type);
            xmlFree(gad);
            continue;
        }
        _pool[rgad] = factoryKnxObject(*this, rgad, reinterpret_cast<char*>(id), reinterpret_cast<char*>(type));
        xmlFree(id);
        xmlFree(type);
        xmlFree(gad);
    }



    /* Cleanup of XPath data */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    if(nng_pub0_open(&_pub) != 0)
    {
        cerr << "Can't open nng pub socket" << endl;
        return;
    }
    if(nng_listen(_pub, "tcp://*:6723", nullptr, 0))
    {
        cerr << "Can't listen nng socket" << endl;
        return;
    }

    if(nng_rep0_open(&_rep) != 0)
    {
        cerr << "Can't open nng req socket" << endl;
    }

    if(nng_listen(_rep, "tcp://*:6722", nullptr, 0))
    {
        cerr << "Can't listen nng socket" << endl;
        return;
    }
}

KnxObjectPool::~KnxObjectPool()
{
    nng_close (_pub);
    nng_close (_rep);
    EIBClose(_connection);
}


void KnxObjectPool::addEventForAll(KnxEventFifo *ev) const
{
    for(auto obj: _pool)
    {
        obj.second->connect(ev);
    }
}

void KnxObjectPool::write(unsigned short src, unsigned short dest, const unsigned char *buf, int len)
{
    UNUSED(src);

    auto obj = _pool.find(dest);
    if (obj != _pool.end())
    {
        switch(buf[1] & 0xC0)
        {
            case 0x40:
                obj->second->knxCmdReponse(std::vector<unsigned char>(buf, buf + len));
                break;
            case 0x80:
                obj->second->knxCmdWrite(std::vector<unsigned char>(buf, buf + len));
                break;
        }

    }
}


bool KnxObjectPool::shutdown() const
{
    return _shutdown;
}

void KnxObjectPool::start()
{
    _thread = thread(
        [this]()
        {
            while(!shutdown())
            {
                eibaddr_t dest;
                eibaddr_t src;
                int len;
                unsigned char buf[200];
                int result;

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 200;
                FD_ZERO (&_read);
                FD_SET (EIB_Poll_FD(_connection), &_read);

                result = select (EIB_Poll_FD (_connection) + 1, &_read, nullptr, nullptr, &tv);
                if(result == 0)
                {
                    continue;
                }

                EIB_Poll_Complete (_connection);
                len = EIBGetGroup_Src (_connection, sizeof (buf), buf, &src, &dest);

                switch (buf[1] & 0xC0)
                {
                    case 0x00:
                        cout << "EIB:Read " << GroupAddressToString(dest) << endl;
                        break;
                    case 0x40:
                    case 0x80:
                        this->write(src, dest, buf, len);
                        break;
                    default:
                        cout << "EIB:unknown" << endl;
                }
            }
        });

    _responder = thread(
        [this]()
        {
            char *repbuf = nullptr;
            size_t sz;

            while(!shutdown())
            {
                /* Reply to nanomsg query */

                int rv = nng_recv(_rep, &repbuf, &sz, NNG_FLAG_ALLOC);
                if(rv == 0)
                {
                    string str(repbuf, sz);
                    nng_free(repbuf, sz);

                    cout << "QUERY: " << str << endl;
                    if (str.rfind("gad?", 0) == 0) {
                        KnxObjectPtr obj = getObjById(str.substr(5));
                        if(obj)
                        {
                            unsigned short gad = htole16(obj->gad());
                            nng_send(_rep, &gad, 2, 0);
                        }
                        else
                        {
                            string rep("object_not_found");
                            nng_send(_rep, const_cast<char*>(rep.data()), rep.length(), 0);
                        }
                    }
                    else if (str.rfind("unity?", 0) == 0) {
                        KnxObjectPtr obj = getObjById(str.substr(7));
                        if(obj)
                        {
                            string unit = obj->unity();
                            nng_send(_rep, const_cast<char*>(unit.data()), unit.length(), 0);
                        }
                        else
                        {
                            string rep("object_not_found");
                            nng_send(_rep, const_cast<char*>(rep.data()), rep.length(), 0);
                        }
                    }
                    else if (str.rfind("data?", 0) == 0)
                    {
                        KnxObjectPtr obj = getObjById(str.substr(6));
                        if(obj)
                        {
                            unsigned char msgp[9];
                            unsigned char type = obj->type();
                            unsigned long long edata = htole64(obj->raw());
                            if(!obj->initialized())
                            {
                                type |= 0x7;
                                obj->knxSendRead();
                            }
                            memcpy (msgp + 0, &type, 1);
                            memcpy (msgp + 1, &edata, 8);
                            nng_send(_rep, msgp, 9, 0);
                        }
                        else
                        {
                            string rep("err:invalid_object");
                            nng_send(_rep, const_cast<char*>(rep.data()), rep.length(), 0);
                        }


                    }
                    else
                    {
                        string rep("err:invalid_query");
                        nng_send(_rep, const_cast<char*>(rep.data()), rep.length(), 0);
                    }
                }
            }
        });
}

void KnxObjectPool::stop()
{
    _shutdown = true;
}

void KnxObjectPool::join()
{
    _thread.join();
}

int KnxObjectPool::send(unsigned short dest, std::vector<unsigned char> data)
{
    return EIBSendGroup(_connection, dest, static_cast<int>(data.size()), data.data());
}

int KnxObjectPool::publish(string topic, string value)
{
    int rc = 0;
    string str = "text." + topic + " " + value;
    cout << "PUB: " << str << endl;
    unique_ptr<unsigned char> msgp(new unsigned char[str.length() + 1]);
    if(msgp)
    {
        memcpy (msgp.get(), str.data(), str.size());
        rc = nng_send(_pub, msgp.get(), str.length(), 0);
    }
    return rc;
}

int KnxObjectPool::publish(unsigned short gad, const KnxDataChanged &data)
{
    /* "b" <u16 GAD> <u8 type> <u64 value> */
    int rc = 0;
    unsigned char msgp[12];
    memcpy (msgp + 0, "b", 1);
    unsigned short egad = htole16(gad);
    memcpy (msgp + 1, &egad, 2);
    unsigned char type = data.data.type;
    memcpy (msgp + 3, &type, 1);
    unsigned long long edata = htole64(data.data.value_unsigned);
    memcpy (msgp + 4, &edata, 8);
    rc = nng_send(_pub, msgp, 12, 0);

    return rc;
}


int KnxObjectPool::getObjIds(std::vector<std::string> &param) const
{
    for(auto obj: _pool)
    {
        const KnxObjectPtr kobj = obj.second;
        param.push_back(kobj->id());
    }
    return static_cast<int>(_pool.size());
}

KnxObjectPtr KnxObjectPool::getObjById(const std::string &id)
{
    for(auto obj: _pool)
    {
        KnxObjectPtr kobj = obj.second;
        if(kobj->id() == id)
            return kobj;
    }
    return nullptr;
}

const KnxObjectPtr KnxObjectPool::getObjByGad(unsigned short addr) const
{
    for(auto obj: _pool)
    {
        const KnxObjectPtr kobj = obj.second;
        if(kobj->gad() == addr)
            return kobj;
    }
    return nullptr;
}
