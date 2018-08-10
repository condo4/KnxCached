#pragma once

#include "knxobject.h"

class KnxObjectSigned32 : public KnxObject
{

public:
    KnxObjectSigned32(KnxObjectPool &pool, unsigned short gad, const std::string &id, unsigned char type_major, unsigned char type_minor);
    virtual ~KnxObjectSigned32();
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);
};
