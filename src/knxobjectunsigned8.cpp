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

KnxObjectUnsigned8::KnxObjectUnsigned8(unsigned short gad, std::string id, unsigned short type_major, unsigned short type_minor):
    KnxObject(gad, id, type_major, type_minor, KnxData::Real)
{
    int idx = _type.minor;
    if(_decode.find(idx) == _decode.end())
    {
        idx = 0;
    }
    _param = &(_decode[idx]);
}

int KnxObjectUnsigned8::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    if(frame.size() != 3)
    {
        cerr << "KnxObjectUnsigned8::_knxDecode size " << frame.size() << " " << id() << endl;
        return -1;
    }

    float fdata = frame[2];
    fdata /= (255.0 / _param->max_value);
    fdata -= _param->min_value;

    if(result.value_real != fdata)
    {
        result.value_real = fdata;
        return 1;
    }
    return 0;
}

void KnxObjectUnsigned8::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    frame.resize(3);
    frame[0] = 0x00;
    frame[1] = 0x00;
    float fdata = data.value_real;
    fdata += _param->min_value;
    fdata *= (255.0 / _param->max_value);

    frame[2] = static_cast<unsigned char>(fdata);
}

string KnxObjectUnsigned8::unity() const
{
    return _decode[_type.minor].unity;
}

string KnxObjectUnsigned8::value() const
{
    stringstream wss;
    wss << _value.value_real;
    return wss.str();
}
