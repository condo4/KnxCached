#include "knxdatachanged.h"

KnxDataChanged::KnxDataChanged()
    : data(KnxData::Unknow)
{

}

KnxDataChanged::KnxDataChanged(const KnxData &ndata)
    : data(ndata)
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
