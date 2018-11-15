#ifndef GADOBJECT_H
#define GADOBJECT_H

#include <vector>
#include <memory>
#include <chrono>
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

std::string GroupAddressToString(unsigned short addr);

class GadObject
{
    eibaddr_t m_gad;
    std::vector<unsigned char> m_data;
    std::chrono::time_point<std::chrono::system_clock> m_lastupdate;
    bool m_valid {false};

    std::vector<ClientConnection *> m_subscriber;
    static std::vector<ClientConnection *> m_dmz;
    static std::vector<GadObject *> m_objects;
    static EIBConnection *m_knxd;
    static eibaddr_t m_individualAddress;


public:
    explicit GadObject(eibaddr_t gad);
    eibaddr_t gad() const;

    /* New API */
    static eibaddr_t IndividualAddress();
    static void setIndividualAddress(eibaddr_t addr);
    static void destroy();

    void fromBus(const std::vector<unsigned char> &data, ClientConnection *sender = nullptr);
    void sendCacheValueToClient(ClientConnection *client);
    void sendToBus(const std::vector<unsigned char> &buffer, ClientConnection *sender = nullptr);


    /* OLD API */
    /*
    void setData(unsigned char command, eibaddr_t src, const std::vector<unsigned char> &data);
    void emitDataChange(unsigned char command, eibaddr_t src);

    void read(eibaddr_t src);
    void sendRead();
    void sendWrite(const std::vector<unsigned char> &data);
    void response(eibaddr_t src, const std::vector<unsigned char> &data);
    void write(eibaddr_t src, const std::vector<unsigned char> &data);
    void memwrite(eibaddr_t src, const std::vector<unsigned char> &data);
    */
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
