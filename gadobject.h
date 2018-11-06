#ifndef GADOBJECT_H
#define GADOBJECT_H

#include <vector>
#include <memory>
#include "eibclient.h"
#include "clientconnection.h"

/*
 * Message Format for notification
 *
 * Message Format:
 *   FLT...
 *
 * F 1 byte: 0xFE (Binary command Type 1)
 * L 1 byte: Len of Message (0 to 255)
 * T 1 byte: Notify Type
 *
 * Type:
 *   0x0X : KNX Message: 2 bytes src_addr, 2 bytes dest_addr, N bytes Payload
 */

#define NOTIFY_KNX_READ     (0b00000000)
#define NOTIFY_KNX_RESPONSE (0b00000001)
#define NOTIFY_KNX_WRITE    (0b00000010)
#define NOTIFY_KNX_MEMWRITE (0b00001010)
#define NOTIFY_EOT          (0b10000001)

std::string GroupAddressToString(unsigned short addr);

class GadObject
{
    eibaddr_t m_gad;
    std::vector<unsigned char> m_data;

    std::vector<ClientConnection *> m_subscriber;
    static std::vector<ClientConnection *> m_dmz;
    static std::vector<GadObject *> m_objects;
    static EIBConnection *m_knxd;

public:
    explicit GadObject(eibaddr_t gad);
    eibaddr_t gad() const;

    void setData(unsigned char command, eibaddr_t src, const std::vector<unsigned char> &data);

    void read(eibaddr_t src);
    void sendRead();
    void sendWrite(const std::vector<unsigned char> &data);
    void response(eibaddr_t src, const std::vector<unsigned char> &data);
    void write(eibaddr_t src, const std::vector<unsigned char> &data);
    void memwrite(eibaddr_t src, const std::vector<unsigned char> &data);
    void sendResponse(ClientConnection *client);
    void removeAll(ClientConnection *client);
    void subscribe(ClientConnection *client);
    void unsubscribe(ClientConnection *client);

    static void forgotClient(ClientConnection *client);
    static GadObject *getObject(eibaddr_t gad);
    static const std::vector<GadObject *> &objects();
    static void dmzSubscribe(ClientConnection *client);
    static void dmzUnsubscribe(ClientConnection *client);
    static void setKnxd(EIBConnection *knxd);

};

#endif // GADOBJECT_H
