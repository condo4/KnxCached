#pragma once

#include "knxobject.h"

class KnxObjectUnsigned8: public KnxObject
{
    struct Param *_param;

public:
    KnxObjectUnsigned8(unsigned short gad, std::string id, unsigned short type_major, unsigned short type_minor);
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);

    virtual std::string unity() const;
    virtual std::string value() const;
};
