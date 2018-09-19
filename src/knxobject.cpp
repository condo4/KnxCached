#include "knxobject.h"

#include <string>
#include <sstream>
#include <iostream>
#include <cassert>

#include "common.h"
#include "knxdata.h"
#include "knxobjectpool.h"
#include "knxeventfifo.h"

using namespace std;

string GroupAddressToString(unsigned short addr)
{
    char buff[10];
    snprintf (buff, 10, "%d/%d/%d", (addr >> 11) & 0x1f, (addr >> 8) & 0x07, (addr) & 0xff);
    buff[9] = '\0';
    return std::string(buff);
}

unsigned short StringToGroupAddress(std::string addr)
{
    unsigned short res = 0;
    unsigned short i = 0;

    istringstream f(addr);
    string s;
    while (getline(f, s, '/')) {
        switch(i++)
        {
        case 0:
            res += stoi(s) << 11;
            break;
        case 1:
            res += stoi(s) << 8;
            break;
        default:
            res += stoi(s);
        }
    }

    return res;
}



KnxObject::KnxObject(KnxObjectPool &pool, unsigned short gad, const string &id, unsigned char type_major, unsigned char type_minor, KnxData::Type type, unsigned char flag)
    : _pool(pool)
    , _gad(gad)
    , _flag(flag)
    , _initialized(false)
    , _id(id)
    , _value(type)
{
    _type.major = type_major;
    _type.minor = type_minor;
}

KnxObject::~KnxObject() {

}


string KnxObject::value() const
{
    stringstream wss;
    switch(_value.type )
    {
    case KnxData::Real:
        wss << _value.value_real;
        break;
    case KnxData::Signed:
        wss << _value.value_signed;
        break;
    case KnxData::Unsigned:
        wss << _value.value_unsigned;
        break;
    default:
        break;
    }
    return wss.str();
}

void KnxObject::setValue(const string &val)
{
    switch(_value.type )
    {
    case KnxData::Real:
        _value.value_real = stod(val);
        break;
    case KnxData::Signed:
        _value.value_signed = stoll(val);
        break;
    case KnxData::Unsigned:
        _value.value_unsigned = stoull(val);
        break;
    default:
        break;
    }

    if(_flag & FLAG_T)
    {
        vector<unsigned char> data;
        _knxEncode(_value, data);
        if(data.size() >= 2) data[1] |= 0x80;
        else {
            cerr << _id << " _knxEncode failed" << endl;
        }
#if defined(DEBUG_KNXOBJECT)
        cout << _id << "::setValue(" << val << ") -> SEND " << data.size() << ": " << hex;
        for(unsigned i = 0; i < data.size(); ++i)
        {
            cout << (int)data[i] << " ";
        }
        cout << endl;
#endif
        _pool.send(gad(), data);
    }
    _initialized = true;
    _valueChanged();
}

void KnxObject::_valueChanged() const
{
    KnxDataChanged data;
    data.data = _value;
    data.id = _id;
    data.knxtype = _type;
    data.unity = unity();
    data.text = value();
    _publish(data);

    for(KnxEventFifo *fifo: _events)
    {
        fifo->push(data);
    }
}

int KnxObject::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    cerr << "Error Generic KnxObject::_knxDecode for " << _id << endl;
    UNUSED(frame);
    UNUSED(result);
    return -1;
}

void KnxObject::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    UNUSED(frame);
    UNUSED(data);
    cerr << "Error Generic KnxObject::_knxEncode for " << _id << endl;
}

void KnxObject::_publish(const KnxDataChanged &data) const
{
    _pool.publish(_id, data.text + " " + data.unity);
    _pool.publish(_gad, data);
}


void KnxObject::knxCmdWrite(const std::vector<unsigned char> &frame)
{
#if defined(DEBUG_KNXOBJECT)
    cout << _id << "->knxCmdWrite()" << endl;
#endif
    if((_flag & FLAG_C) && (_flag & FLAG_W))
    {
        int res = _knxDecode(frame, _value);
        if(res < 0)
        {
            cerr << "Error KnxObject::knxCmdWrite";
        }
        if(res)
        {
            if(!_initialized) _initialized = true;
            _valueChanged();
        }
    }
}

void KnxObject::knxCmdReponse(const std::vector<unsigned char> &frame)
{
#if defined(DEBUG_KNXOBJECT)
    cout << _id << "->knxCmdReponse()" << endl;
#endif
    if((_flag & FLAG_C) && (_flag & FLAG_U))
    {
        int res = _knxDecode(frame, _value);
        if(res < 0)
        {
            cerr << "Error KnxObject::knxCmdReponse for " << _id << endl;
        }
        if(res)
        {
            if(!_initialized) _initialized = true;
            _valueChanged();
        }
    }
}

void KnxObject::knxCmdRead(const std::vector<unsigned char> &frame)
{
#if defined(DEBUG_KNXOBJECT)
    cout << _id << "->knxCmdRead()" << endl;
#endif
    UNUSED(frame);
    if((_flag & FLAG_C) && (_flag & FLAG_R))
    {
        vector<unsigned char> data;
        _knxEncode(_value, data);
        data[1] |= 0x40;
        _pool.send(gad(), data);
    }
}

void KnxObject::knxSendRead() const
{
#if defined(DEBUG_KNXOBJECT)
    cout << _id << "->knxSendRead()" << endl;
#endif
    vector<unsigned char> data;
    data.resize(2);
    data[0] = 0x00;
    _pool.send(gad(), data);
}





unsigned short KnxObject::gad() const
{
    return _gad;
}

const string& KnxObject::id() const
{
    return _id;
}

string KnxObject::unity() const
{
    return "";
}


void KnxObject::connect(KnxEventFifo *ev) const
{
    _events.push_back(ev);
    if(!_initialized && (_flag & FLAG_I))
    {
        knxSendRead();
    }
    else
    {
        _valueChanged();
    }
}

void KnxObject::disconnect(KnxEventFifo *ev) const
{
    _events.remove(ev);
}


bool KnxObject::initialized() const
{
    return _initialized;
}




#include "knxobjectbool.h"       /* 01 Switch    1bit */
                                 /* 02                */
#include "knxobject3bits.h"      /* 03 Dimming   3bit */
                                 /* 04                */
#include "knxobjectunsigned8.h"  /* 05 Unsigned  8bit */
                                 /* 06                */
#include "knxobjectunsigned16.h" /* 07 Unsigned 16bit */
                                 /* 08                */
#include "knxobjectfloat16.h"    /* 09 Float    16bit */
#include "knxobjectsigned32.h"   /* 13 Signed   32bit */
#include "knxobjectfloat.h"      /* 14 Float    32bit */


KnxObjectPtr factoryKnxObject(KnxObjectPool &pool, unsigned short gad, string id, const char *type)
{
    unsigned char type_major = 0;
    unsigned char type_minor = 0;

    if(type)
    {
        unsigned short i = 0;
        istringstream f(type);
        string s;
        while (getline(f, s, '.')) {
            switch(i++)
            {
            case 0:
                type_major = static_cast<unsigned char>(stoi(s));
                break;
            case 1:
                if(s[0] == 'x')
                {
                    type_minor = 0;
                }
                else
                {
                    type_minor = static_cast<unsigned char>(stoi(s));
                }
                break;
            default:
                cerr << "ERROR TYPE PARSING" << endl;
                return nullptr;
            }
        }
    }
    switch(type_major)
    {
    case 1:
        return KnxObjectPtr(new KnxObjectBool(pool, gad, id, type_major, type_minor));
    case 3:
        return KnxObjectPtr(new KnxObject3Bits(pool, gad, id, type_major, type_minor));
    case 5:
        return KnxObjectPtr(new KnxObjectUnsigned8(pool, gad, id, type_major, type_minor));
    case 7:
        return KnxObjectPtr(new KnxObjectUnsigned16(pool, gad, id, type_major, type_minor));
    case 9:
        return KnxObjectPtr(new KnxObjectFloat16(pool, gad, id, type_major, type_minor));
    case 13:
        return KnxObjectPtr(new KnxObjectSigned32(pool, gad, id, type_major, type_minor));
    case 14:
        return KnxObjectPtr(new KnxObjectFloat(pool, gad, id, type_major, type_minor));
    default:
        return KnxObjectPtr(new KnxObject(pool, gad, id, type_major, type_minor));
    }
}
