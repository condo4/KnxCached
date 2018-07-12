#include "knxobjectfloat.h"

#include <cassert>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "knxobjectpool.h"

using namespace std;

KnxObjectFloat::KnxObjectFloat(unsigned short gad, string id, unsigned char type_major, unsigned char type_minor):
    KnxObject(gad, id, type_major, type_minor, KnxData::Real)
{
}

KnxObjectFloat::~KnxObjectFloat()
{

}

int KnxObjectFloat::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    if(frame.size() != 6)
    {
        cerr << "KnxObjectFloat::_knxDecode size " << frame.size() << " " << id() << endl;
        return -1;
    }
    int data = ((frame[2]<< 24) | (frame[3]<< 16) | (frame[4]<< 8) | frame[5]);
    float fdata;
    assert(sizeof data == sizeof fdata);
    memcpy(&fdata, &data, sizeof data);
    double newval = static_cast<double>(fdata);
    if(!CompareDoubles(newval, result.value_real))
    {
        result.value_real = newval;
        return 1;
    }
    return 0;
}

void KnxObjectFloat::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    float fdata = static_cast<float>(data.value_real);
    unsigned int raw;
    assert(sizeof raw == sizeof fdata);
    memcpy(&raw, &fdata, sizeof fdata);
    frame.resize(6);
    frame[0] = 0x00;
    frame[1] = 0x00;
    frame[2] = (raw >> 24) & 0xFF;
    frame[3] = (raw >> 16) & 0xFF;
    frame[4] = (raw >> 8) & 0xFF;
    frame[5] = (raw >> 0) & 0xFF;
}

string KnxObjectFloat::unity() const
{
    switch(_type.minor)
    {
    case 56:
        return "W";
    default:
        return string(" unit(") + to_string(_type.major) + "."  + to_string(_type.minor) + ")";
    }
}
