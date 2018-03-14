#include "knxobjectunsigned16.h"

#include <cassert>
#include <iostream>
#include <map>

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
    {0,  {"Generic", "", 0, 255}}
};


KnxObjectUnsigned16::KnxObjectUnsigned16(unsigned short gad, std::string id, unsigned short type_major, unsigned short type_minor):
    KnxObject(gad, id, type_major, type_minor, KnxData::Unsigned)
{

}

int KnxObjectUnsigned16::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    if(frame.size() != 4)
    {
        cerr << "KnxObjectUnsigned16::_knxDecode size " << frame.size() << " " << id() << endl;
        return -1;
    }
    unsigned short val = (((frame[2]<< 8) | (frame[3])));
    if(val != result.value_unsigned)
    {
        result.value_unsigned = val;
        return 1;
    }
    return 0;
}

void KnxObjectUnsigned16::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    int id = _type.minor;
    if(_decode.find(id) == _decode.end())
    {
        id = 0;
    }
    float fdata = data.value_unsigned;
    fdata += _decode[id].min_value;
    fdata *= (255.0 / _decode[id].max_value);
    unsigned short raw = (int)fdata;
    frame.resize(3);
    frame[0] = 0x00;
    frame[1] = 0x00;
    frame[2] = raw >> 8;
    frame[3] = raw & 0xFF;
}

std::string KnxObjectUnsigned16::unity() const
{
    return _decode[_type.minor].unity;
}
