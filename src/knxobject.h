#pragma once

#include <string>
#include <list>
#include <queue>

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


class KnxData
{
public:
    enum Type {
        Unknow,
        Signed,
        Unsigned,
        Real
    };
    Type type;
    union {
        double value_real;
        signed long long value_signed;
        unsigned long long value_unsigned;
    };

    KnxData(Type t):
        type(t),
        value_unsigned(0)
    {}
};


struct knx_type
{
    unsigned char major;
    unsigned char minor;
};



class KnxDataChanged
{
public:
    KnxData data;
    struct knx_type knxtype;
    std::string id;
    std::string unity;
    std::string text;


    KnxDataChanged():
        data(KnxData::Unknow) {}

    KnxDataChanged(const KnxData &ndata):
        data(ndata) {}

    KnxDataChanged(const KnxDataChanged &src):
        data(src.data),
        knxtype(src.knxtype),
        id(src.id),
        unity(src.unity),
        text(src.text)
    {}
};


class KnxObject
{
private:
    unsigned short _gad;
    std::string _id;
    mutable std::list<KnxEventFifo*> _events;
    unsigned char _flag;
    bool _initialized;

    void _valueChanged() const;

protected:
    KnxData _value;
    struct knx_type _type;

    /* Return 0 if nothing, 1 if change, -1 if error */
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);


public:
    KnxObject(unsigned short gad, std::string id, unsigned short type_major, unsigned short type_minor, KnxData::Type type = KnxData::Unknow, unsigned char flag = FLAG_DEF);
    virtual ~KnxObject() {}

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
    virtual void setValue(const std::string val);
    bool initialized() const;
};

KnxObject *factoryKnxObject(unsigned short gad, std::string id, const char *type);
