#include "knxobjectunsigned8.h"

#include <cassert>
#include <iostream>
#include <map>
#include <sstream>

#include "knxobjectpool.h"

using namespace std;

struct Param
{
    string name;
    string unity;
    int min_value;
    int max_value;
};

static map<unsigned short, struct Param> _decode = {
    {0,  {"Generic", "", 0, 255}},
    {1,  {"Scaling", "%", 0, 100}},
    {3,  {"Angle", "°", 0, 360}},
    {4,  {"Percent", "%", 0, 100}},
    {5,  {"DecimalFactor", "ratio", 0, 1}}
};

KnxObjectUnsigned8::KnxObjectUnsigned8(KnxObjectPool &pool, unsigned short gad, const string &id, unsigned char type_major, unsigned char type_minor)
    : KnxObject(pool, gad, id, type_major, type_minor, KnxData::Real)
    , _idx(_type.minor)
{
    if(_decode.find(_idx) == _decode.end())
    {
        _idx = 0;
    }
}

KnxObjectUnsigned8::~KnxObjectUnsigned8()
{

}

int KnxObjectUnsigned8::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    if(frame.size() != 3)
    {
        cerr << "KnxObjectUnsigned8::_knxDecode size " << frame.size() << " " << id() << endl;
        return -1;
    }

    double fdata = frame[2];
    fdata /= (255.0 / _decode[_idx].max_value);
    fdata -= _decode[_idx].min_value;

    unsigned short val = static_cast<unsigned short>(fdata);
    if(val != result.value_unsigned)
    {
        result.value_unsigned = val;
        return 1;
    }
    return 0;
}

void KnxObjectUnsigned8::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    frame.resize(3);
    frame[0] = 0x00;
    frame[1] = 0x00;
    double fdata = data.value_real;
    fdata += _decode[_idx].min_value;
    fdata *= (255.0 / _decode[_idx].max_value);

    frame[2] = static_cast<unsigned char>(fdata);
}

string KnxObjectUnsigned8::unity() const
{
    return _decode[_idx].unity;
}

string KnxObjectUnsigned8::value() const
{
    stringstream wss;
    wss << _value.value_unsigned;
    return wss.str();
}
