#pragma once

#include "knxobject.h"

class KnxObjectFloat16 : public KnxObject
{

public:
    KnxObjectFloat16(unsigned short gad, const std::string &id, unsigned char type_major, unsigned char type_minor);
    virtual ~KnxObjectFloat16();
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);

    virtual std::string unity() const;
};
