#pragma once


struct knx_type
{
    unsigned char major;
    unsigned char minor;
};


class KnxData
{
public:
    enum Type {
        Unknow,
        Signed,
        Unsigned,
        Real
    };
    Type type;
    union {
        double value_real;
        signed long long value_signed;
        unsigned long long value_unsigned;
    };

    KnxData(Type t);

    virtual ~KnxData();
};
