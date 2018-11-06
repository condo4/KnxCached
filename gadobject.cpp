#include "gadobject.h"

#include <iostream>
#include <iomanip>
#include <algorithm>



std::vector<ClientConnection *> GadObject::m_dmz;
std::vector<GadObject *> GadObject::m_objects;
EIBConnection *GadObject::m_knxd = nullptr;

std::string GroupAddressToString(unsigned short addr)
{
    char buff[10];
    snprintf (buff, 10, "%d/%d/%d", (addr >> 11) & 0x1f, (addr >> 8) & 0x07, (addr) & 0xff);
    buff[9] = '\0';
    return std::string(buff);
}


GadObject::GadObject(eibaddr_t gad)
    : m_gad(gad)
{
    std::cout << "[KNX] Create KNX Object " << GroupAddressToString(gad) << std::endl;
}

eibaddr_t GadObject::gad() const
{
    return m_gad;
}

void GadObject::setData(unsigned char command, eibaddr_t src, const std::vector<unsigned char> &data)
{
    m_data = data;
    std::vector<unsigned char> message;
    message.push_back(0xFE);
    message.push_back(0x00); /* LEN TO SET AT THE END */
    message.push_back(command);
    message.push_back((src >> 8) & 0xFF);
    message.push_back(src & 0xFF);
    message.push_back((m_gad >> 8) & 0xFF);
    message.push_back(m_gad & 0xFF);
    for(unsigned char c:  m_data)
    {
        message.push_back(c);
    }
    message[1] = static_cast<unsigned char>(message.size() - 3);

    if((command & 0xF0) == 0x00)
    {
        /* KNX command */
        for(ClientConnection *client: m_subscriber)
        {
            client->write(message);
        }
        for(ClientConnection *client: m_dmz)
        {
            client->write(message);
        }
    }
    else
    {
        std::cout << "? COMMAND" << (command & 0xF0) << std::endl;
    }
}

void GadObject::read(eibaddr_t src)
{
    std::cout << "[KNX] Read: " << GroupAddressToString(m_gad) << std::endl;

}

void GadObject::sendRead()
{
    std::cout << "[KNX] Send Read: " << GroupAddressToString(m_gad) << std::endl;
    const uint8_t message[] = {0x00, 0x00};
    EIBSendGroup(m_knxd, m_gad, 2, message);
}

void GadObject::sendWrite(const std::vector<unsigned char> &data)
{
    std::cout << "[KNX] Send Write:" << GroupAddressToString(m_gad) << "\t";
    for(unsigned char c: data)
        std::cout << std::setfill('0') << std::setw(2) << std::hex << int(c) << std::dec << ":";
    std::cout << "\b " << std::endl;

    EIBSendGroup(m_knxd, m_gad, data.size(), data.data());
}


void GadObject::response(eibaddr_t src, const std::vector<unsigned char> &data)
{
    std::cout << "[KNX] Response:" << GroupAddressToString(m_gad) << std::endl;
    if(m_data.size() == 0 || m_data != data)
    {
        setData(NOTIFY_KNX_RESPONSE, src, data);
    }
}

void GadObject::write(eibaddr_t src, const std::vector<unsigned char> &data)
{
    std::cout << "[KNX] Write:" << GroupAddressToString(m_gad) << "\t";
    for(unsigned char c: data)
        std::cout << std::setfill('0') << std::setw(2) << std::hex << int(c) << std::dec << ":";
    std::cout << "\b " << std::endl;

    if(m_data.size() == 0 || m_data != data)
    {
        setData(NOTIFY_KNX_WRITE, src, data);
    }
}

void GadObject::memwrite(eibaddr_t src, const std::vector<unsigned char> &data)
{
    std::cout << "[KNX] Memory Write:" << GroupAddressToString(m_gad) << "\t";
    for(unsigned char c: data)
        std::cout << std::setfill('0') << std::setw(2) << std::hex << int(c) << std::dec << ":";
    std::cout << "\b " << std::endl;
}

void GadObject::sendResponse(ClientConnection *client)
{
    std::vector<unsigned char> message;
    message.push_back(0xFE);
    message.push_back(0x00); /* LEN TO SET AT THE END */
    message.push_back(NOTIFY_KNX_RESPONSE);
    message.push_back(0);
    message.push_back(0);
    message.push_back((m_gad >> 8) & 0xFF);
    message.push_back(m_gad & 0xFF);
    for(unsigned char c:  m_data)
        message.push_back(c);
    message[1] = static_cast<unsigned char>(message.size() - 3);
    std::cout << "[KNX] " << GroupAddressToString(m_gad) << " send response to " << client->sd() << std::endl;
    client->write(message);
}

void GadObject::removeAll(ClientConnection *client)
{
    m_subscriber.erase(std::remove(m_subscriber.begin(), m_subscriber.end(), client), m_subscriber.end());
}

void GadObject::subscribe(ClientConnection *client)
{
    m_subscriber.push_back(client);
    if(m_data.size())
    {
        sendResponse(client);
    }
    else
    {
        sendRead();
    }
}

void GadObject::unsubscribe(ClientConnection *client)
{
    m_subscriber.erase(std::remove(m_subscriber.begin(), m_subscriber.end(), client), m_subscriber.end());
}

void GadObject::forgotClient(ClientConnection *client)
{
    m_dmz.erase(std::remove(m_dmz.begin(), m_dmz.end(), client), m_dmz.end());
    for(GadObject *obj: m_objects)
    {
        obj->removeAll(client);
    }
}

GadObject *GadObject::getObject(eibaddr_t gad)
{
    for(GadObject *obj: m_objects)
    {
        if(obj->gad() == gad)
            return obj;
    }
    GadObject *obj = new GadObject(gad);
    m_objects.push_back(obj);
    return obj;
}

const std::vector<GadObject *> &GadObject::objects()
{
    return m_objects;
}

void GadObject::dmzSubscribe(ClientConnection *client)
{
    m_dmz.push_back(client);
}

void GadObject::dmzUnsubscribe(ClientConnection *client)
{
    m_dmz.erase(std::remove(m_dmz.begin(), m_dmz.end(), client), m_dmz.end());
}

void GadObject::setKnxd(EIBConnection *knxd)
{
    m_knxd = knxd;
}

