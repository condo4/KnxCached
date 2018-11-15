#ifndef COMMAND_TYPE_H
#define COMMAND_TYPE_H

#include <vector>
#include <iostream>
#include <iomanip>

#define TELEGRAM_BEGIN      (0xC5)
#define TELEGRAM_END        (0x5C)
#define TELEGRAM_EOT        (0x80)

#define KNX_READ            (0x00)
#define KNX_RESPONSE        (0x01)
#define KNX_WRITE           (0x02)
#define KNX_MEMWRITE        (0x0A)
#define KNX_CACHE_VALUE     (0x11)

#define CMD_SUBSCRIBE       (0x81)
#define CMD_UNSUBSCRIBE     (0x82)
#define CMD_SUBSCRIBE_DMZ   (0x83)
#define CMD_UNSUBSCRIBE_DMZ (0x84)
#define CMD_DUMP_CACHED     (0x85)
#define CMD_REQUEST_VALUE   (0x86)




#define CMD_SETDEBUG        (0x88)
#define CMD_SEND_WRITE      (0x87)
#define CMD_READ            (0x88)
#define CMD_SEND_RESPONSE   (0x8A)


inline void dump_telegram(const std::vector<unsigned char> &message)
{
    std::cout << "[" << std::hex << std::setfill('0') << std::setw(2);
    for(const auto &i: message)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << int(i) << ":";
    }
    std::cout << "\b]" << std::dec;
}

#endif // COMMAND_TYPE_H
