#include "knxobjectpool.h"

#include <sstream>
#include <iostream>
#include <string>
#include <thread>
#include <cassert>

#include <libxml/encoding.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>


#include "eibclient.h"

using namespace std;

KnxObjectPool::KnxObjectPool(void *context, string conffile)
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

    _pub = zmq_socket(context, ZMQ_PUB);
    zmq_bind(_pub, "tcp://*:5563");
}

KnxObjectPool::~KnxObjectPool()
{
    zmq_close (_pub);
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
                tv.tv_sec = 1;
                tv.tv_usec = 0;
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
    string str = topic + " " + value;
    cout << "PUB: " << str << endl;
    zmq_msg_t message;
    zmq_msg_init_size (&message, str.length());
    memcpy (zmq_msg_data (&message), str.data(), str.size());
    int rc = zmq_msg_send (&message, _pub, 0);
    zmq_msg_close (&message);
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
