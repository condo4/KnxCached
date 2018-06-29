#pragma once

#include <string>
#include "knxdata.h"

class KnxDataChanged
{
public:
    KnxData data;
    struct knx_type knxtype;
    std::string id;
    std::string unity;
    std::string text;


    KnxDataChanged();
    KnxDataChanged(const KnxData &ndata);
    KnxDataChanged(const KnxDataChanged &src);
};
