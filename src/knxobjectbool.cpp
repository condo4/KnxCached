#include "knxobjectbool.h"

#include <map>
#include <vector>
#include <iostream>
#include <cassert>

#include "knxobjectpool.h"

using namespace std;

static map<unsigned short, vector<string>> _decode = {
    {1,  {"Off",               "On",                        "Switch"}},
    {2,  {"False",             "True",                      "Boolean"}},
    {3,  {"Disable",           "Enable",                    "Enable"}},
    {4,  {"No ramp",           "Ramp",                      "Ramp"}},
    {5,  {"No alarm",          "Alarm",                     "Alarm"}},
    {6,  {"Low",               "High",                      "Binary"}},
    {7,  {"Decrease",          "Increase",                  "Step"}},
    {8,  {"Up",                "Down",                      "Up/Down"}},
    {9,  {"Open",              "Close",                     "Open/Close"}},
    {10, {"Stop",              "Start",                     "Start"}},
    {11, {"Inactive",          "Active",                    "State"}},
    {12, {"Not inverte",       "Inverted",                  "Invert"}},
    {13, {"Start/stop",        "Cyclically",                "Dimmer send-style"}},
    {14, {"Fixed",             "Calculated",                "Input source"}},
    {15, {"No action",         "Reset",                     "Reset"}},
    {16, {"No action",         "Acknowledge",               "Acknowledge"}},
    {17, {"0",                 "1",                         "Trigger"}},
    {18, {"Not occupied",      "Occupied",                  "Occupancy"}},
    {19, {"Closed",            "Open",                      "Window/Door"}},
    {21, {"OR",                "AND",                       "Logical function"}},
    {22, {"Scene A",           "Scene B",                   "Scene A/B"}},
    {23, {"Only move Up/Down", "Move Up/Down + StepStop",   "Shutter/Blinds mode"}}
};

KnxObjectBool::KnxObjectBool(unsigned short gad, string id, unsigned char type_major, unsigned char type_minor):
    KnxObject(gad, id, type_major, type_minor, KnxData::Unsigned)
{
}

KnxObjectBool::~KnxObjectBool()
{

}

int KnxObjectBool::_knxDecode(const std::vector<unsigned char> &frame, KnxData &result)
{
    if(frame.size() != 2)
    {
        cerr << "KnxObjectBool::_knxDecode size " << frame.size() << " " << id() << endl;
        return -1;
    }
    if(result.value_unsigned != (frame[1] & 0x1))
    {
        result.value_unsigned = frame[1] & 0x1;
        return 1;
    }
    return 0;
}

void KnxObjectBool::_knxEncode(const KnxData &data, std::vector<unsigned char> &frame)
{
    frame.resize(2);
    frame[0] = 0x00;
    frame[1] = 0x00;
    unsigned char cmd = 0;
    cmd = cmd & 0xFE;
    cmd |= (data.value_unsigned)?(0x1):(0x0);
    frame[1] = cmd;
}
