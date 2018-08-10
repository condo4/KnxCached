#pragma once

#include <string>
#include <list>
#include <queue>
#include <memory>

#include "knxdata.h"

#define EPSILON 0.000001
inline bool CompareDoubles (double A, double B)
{
   double diff = A - B;
   return (diff < EPSILON) && (-diff < EPSILON);
}

// #define DEBUG_KNXOBJECT

#define UNUSED(x) (void)(x)

std::string GroupAddressToString(unsigned short addr);
unsigned short StringToGroupAddress(std::string addr);

#define FLAG_C 1
#define FLAG_R 2
#define FLAG_W 4
#define FLAG_T 8
#define FLAG_U 16
#define FLAG_I 32
#define FLAG_DEF (FLAG_C | FLAG_W | FLAG_T | FLAG_U | FLAG_I)

class KnxEventFifo;



class KnxObject
{
private:
    unsigned short _gad;
    unsigned char _flag;
    bool _initialized;
    std::string _id;
    mutable std::list<KnxEventFifo*> _events;

    void _valueChanged() const;

protected:
    KnxData _value;
    struct knx_type _type;

    /* Return 0 if nothing, 1 if change, -1 if error */
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);


public:
    KnxObject(unsigned short gad, const std::string &id, unsigned char type_major, unsigned char type_minor, KnxData::Type type = KnxData::Unknow, unsigned char flag = FLAG_DEF);
    virtual ~KnxObject();

    void knxCmdWrite(const std::vector<unsigned char> &frame);
    void knxCmdReponse(const std::vector<unsigned char> &frame);
    void knxCmdRead(const std::vector<unsigned char> &frame);

    void knxSendRead() const;

    void connect(KnxEventFifo* ev) const;
    void disconnect(KnxEventFifo* ev) const;

    unsigned short gad() const;
    const std::string &id() const;
    virtual std::string unity() const;
    virtual std::string value() const;
    virtual void setValue(const std::string &val);
    bool initialized() const;
};


typedef std::shared_ptr<KnxObject> KnxObjectPtr;

KnxObject *factoryKnxObject(unsigned short gad, std::string id, const char *type);
