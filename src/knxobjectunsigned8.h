#pragma once

#include "knxobject.h"

class KnxObjectUnsigned8: public KnxObject
{
    unsigned short _idx;

public:
    KnxObjectUnsigned8(KnxObjectPool &pool, unsigned short gad, const std::string &id, unsigned char type_major, unsigned char type_minor);
    virtual ~KnxObjectUnsigned8();
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);

    virtual std::string unity() const;
    virtual std::string value() const;
};
