#include "knxobjectsigned32.h"

#include <cassert>
#include <iostream>
using namespace std;


KnxObjectSigned32::KnxObjectSigned32(unsigned short gad, const string &id, unsigned char type_major, unsigned char type_minor):
    KnxObject(gad, id, type_major, type_minor, KnxData::Signed)
{

}

KnxObjectSigned32::~KnxObjectSigned32()
{

}

int KnxObjectSigned32::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    if(frame.size() != 6)
    {
        cerr << "KnxObjectSigned32::_knxDecode size " << frame.size() << " " << id() << endl;
        return -1;
    }
    int val = ((frame[2]<< 24) | (frame[3]<< 16) | (frame[4]<< 8) | frame[5]);
    if(val != result.value_signed)
    {
        result.value_signed = val;
        return 1;
    }
    return 0;
}

void KnxObjectSigned32::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    int val = static_cast<signed int>(data.value_signed);
    frame.resize(6);
    frame[0] = 0x00;
    frame[1] = 0x00;
    frame[2] = (val >> 24) & 0xFF;
    frame[3] = (val >> 16) & 0xFF;
    frame[4] = (val >> 8) & 0xFF;
    frame[5] = (val >> 0) & 0xFF;
}
