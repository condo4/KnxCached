#include "knxobject3bits.h"
#include <cassert>
#include <iostream>

#include "knxobjectpool.h"

using namespace std;

KnxObject3Bits::KnxObject3Bits(KnxObjectPool &pool, unsigned short gad, const std::string &id, unsigned char type_major, unsigned char type_minor):
    KnxObject(pool, gad, id, type_major, type_minor, KnxData::Signed)
{
}

KnxObject3Bits::~KnxObject3Bits()
{

}

int KnxObject3Bits::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    if(frame.size() != 2)
    {
        cerr << "KnxObject3Bits::_knxDecode size " << frame.size() << " " << id() << endl;
        return -1;
    }
    int val = (frame[1] & 0x7);
    int res = ((frame[1] & 0x8)?(val):(-val));
    if(res != result.value_signed)
    {
        result.value_signed = res;
        return 1;
    }
    return 0;
}

void KnxObject3Bits::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    int raw = static_cast<int>(data.value_signed);
    frame.resize(2);
    frame[0] = 0x00;
    frame[1] = 0x00;
    if(raw < 0)
        frame[1] |= 0x8;
    frame[1] |= raw & 0x7;
}
