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

KnxObjectPool* KnxObjectPool::_instance = NULL;

KnxObjectPool::KnxObjectPool(string conffile):
    _shutdown(false)
{
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlDocPtr doc;
    string res;
    string query;

    xmlInitParser();

    doc = xmlParseFile(conffile.c_str());
    if (doc == NULL)
    {
        cerr << "Error: unable to parse file " << conffile << endl;
        exit(1);
    }

    xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == NULL)
    {
        cerr << "Error: unable to create new XPath context" << endl;
        exit(1);
    }

    /* Connect to knxd */
    xpathObj = xmlXPathEvalExpression((const xmlChar*)"//knxd", xpathCtx);
    if(xpathObj == NULL) {
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
    xmlChar *url = xmlGetProp(knxdnode, (const xmlChar*)"url");
    _connection = EIBSocketURL (reinterpret_cast<char*>(url));
    if (EIBOpen_GroupSocket (_connection, 0) == -1){
        std::cout << "Error opening knxd socket (" << reinterpret_cast<char*>(url) << ")" << std::endl;
        xmlXPathFreeContext(xpathCtx);
        exit(1);
    }
    xmlFree(url);
    xmlXPathFreeObject(xpathObj);

    /* Create all objects */
    xpathObj = xmlXPathEvalExpression((const xmlChar*)"//object", xpathCtx);
    if(xpathObj == NULL) {
        cerr << "Error: unable to evaluate xpath expression" << endl;
        xmlXPathFreeContext(xpathCtx);
        exit(1);
    }

    for(int i = 0; i < xpathObj->nodesetval->nodeNr; i++)
    {
        xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
        xmlChar *id = xmlGetProp(node, (const xmlChar*)"id");
        xmlChar *type = xmlGetProp(node, (const xmlChar*)"type");
        xmlChar *gad = xmlGetProp(node, (const xmlChar*)"gad");
        unsigned short rgad = StringToGroupAddress(reinterpret_cast<char*>(gad));
        if(_pool.find(rgad) != _pool.end())
        {
            cerr << "Error: There is 2 object with " << GroupAddressToString(rgad) << " key" << endl;
            xmlFree(id);
            xmlFree(type);
            xmlFree(gad);
            continue;
        }
        _pool[rgad] = factoryKnxObject(rgad, reinterpret_cast<char*>(id), reinterpret_cast<char*>(type));
        xmlFree(id);
        xmlFree(type);
        xmlFree(gad);
    }

    /* Cleanup of XPath data */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    _instance = this;
}

KnxObjectPool::~KnxObjectPool()
{
    for(auto obj: _pool)
    {
        delete obj.second;
    }
    EIBClose(_connection);
}



KnxObjectPool *KnxObjectPool::instance()
{
    return _instance;
}

void KnxObjectPool::addEventForAll(KnxEventFifo *ev) const
{
    for(auto obj: _pool)
    {
        obj.second->connect(ev);
    }
}

void KnxObjectPool::write(unsigned short src, unsigned short dest, const unsigned char *buf, unsigned short len)
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

                result = select (EIB_Poll_FD (_connection) + 1, &_read, 0, 0, &tv);
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
    return EIBSendGroup(_connection, dest, data.size(), data.data());
}

int KnxObjectPool::getObjIds(std::vector<const char *> &param) const
{
    for(auto obj: _pool)
    {
        const KnxObject *kobj = obj.second;
        param.push_back(kobj->id().c_str());
    }
    return _pool.size();
}

KnxObject *KnxObjectPool::getObjById(const char *id)
{
    for(auto obj: _pool)
    {
        KnxObject *kobj = obj.second;
        if(kobj->id() == string(id))
            return kobj;
    }
    return NULL;
}

KnxObject *KnxObjectPool::getObjById(const string& id)
{
    return getObjById(id.c_str());
}

const KnxObject *KnxObjectPool::getObjByGad(unsigned short addr) const
{
    for(auto obj: _pool)
    {
        const KnxObject *kobj = obj.second;
        if(kobj->gad() == addr)
            return kobj;
    }
    return NULL;
}
