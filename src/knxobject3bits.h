#pragma once

#include "knxobject.h"

class KnxObject3Bits : public KnxObject
{

public:
    KnxObject3Bits(unsigned short gad, const std::string &id, unsigned char type_major, unsigned char type_minor);
    virtual ~KnxObject3Bits();
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);
};
