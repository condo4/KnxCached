#pragma once

#include "knxobject.h"

class KnxObjectSigned32 : public KnxObject
{

public:
    KnxObjectSigned32(unsigned short gad, std::string id, unsigned short type_major, unsigned short type_minor);
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);
};
