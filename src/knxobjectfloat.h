#pragma once

#include "knxobject.h"

class KnxObjectFloat : public KnxObject
{

public:
    KnxObjectFloat(unsigned short gad, std::string id, unsigned short type_major, unsigned short type_minor);
    virtual int _knxDecode(const std::vector<unsigned char> &frame, KnxData &result);
    virtual void _knxEncode(const KnxData &data, std::vector<unsigned char> &frame);

    virtual std::string unity() const;
};
