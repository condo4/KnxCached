#include "knxdatachanged.h"

KnxDataChanged::KnxDataChanged()
    : data(KnxData::Unknow)
    , knxtype({0,0})

{

}

KnxDataChanged::KnxDataChanged(const KnxData &ndata)
    : data(ndata)
    , knxtype({0,0})
{

}

KnxDataChanged::KnxDataChanged(const KnxDataChanged &src)
    : data(src.data)
    , knxtype(src.knxtype)
    , id(src.id)
    , unity(src.unity)
    , text(src.text)
{

}
