#pragma once

#include "knxobject.h"

class KnxObjectBool : public KnxObject
{

public:
    KnxObjectBool(KnxObjectPool &pool, unsigned short gad, const std::string &id, unsigned char type_major, unsigned char type_minor);
    virtual ~KnxObjectBool();
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);
};
