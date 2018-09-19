#include "knxobjectfloat16.h"

#include <cassert>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <cstring>

#include "knxobjectpool.h"

using namespace std;



KnxObjectFloat16::KnxObjectFloat16(KnxObjectPool &pool, unsigned short gad, const string &id, unsigned char type_major, unsigned char type_minor):
    KnxObject(pool, gad, id, type_major, type_minor, KnxData::Real)
{
}

KnxObjectFloat16::~KnxObjectFloat16()
{

}

int KnxObjectFloat16::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    if(frame.size() != 4)
    {
        cerr << "KnxObjectFloat16::_knxDecode size " << frame.size() << " " << id() << endl;
        return -1;
    }
    double res;
    unsigned short data = static_cast<unsigned short>((frame[2] << 8) | frame[3]);
    unsigned short sign = (data & 0x8000) >> 15;
    unsigned short exp = (data & 0x7800) >> 11;
    int mant = data & 0x07ff;
    if(sign != 0)
        mant = -(~(mant - 1) & 0x07ff);
    res = (1 << exp) * 0.01 * mant;
    if(!CompareDoubles(res, result.value_real))
    {
        result.value_real = res;
        return 1;
    }
    return 0;
}

void KnxObjectFloat16::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    float fdata = static_cast<float>(data.value_real);
    unsigned int sign;
    unsigned int exp;
    unsigned short raw;
    int mant;
    sign = 0;
    exp = 0;
    if (fdata < 0)
        sign = 1;
    mant = static_cast<int>(fdata * 100);
    while(mant > 2047 || mant < -2048)
    {
        mant = mant >> 1;
        ++exp;
    }
    raw = static_cast<unsigned short>((sign << 15) | (exp << 11) | (mant & 0x07ff));
    frame.resize(4);
    frame[0] = 0x00;
    frame[1] = 0x00;
    frame[2] = (raw >> 8) & 0xFF;
    frame[3] = (raw >> 0) & 0xFF;
}

string KnxObjectFloat16::unity() const
{
    switch(_type.minor)
    {
    case 1:
        return "°C";
    case 4:
        return "Lux";
    case 7:
        return "%";
    case 24:
        return "kW";
    default:
        return "";
    }
}
