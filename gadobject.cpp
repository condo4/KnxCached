#include "gadobject.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include "command_type.h"

std::vector<ClientConnection *> GadObject::m_dmz;
std::vector<GadObject *> GadObject::m_objects;
EIBConnection *GadObject::m_knxd = nullptr;
eibaddr_t GadObject::m_individualAddress = 0;

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
#if defined(DEBUG)
    std::cout << "[KNX] Create KNX Object " << GroupAddressToString(gad) << std::endl;
#endif
}

eibaddr_t GadObject::gad() const
{
    return m_gad;
}

eibaddr_t GadObject::IndividualAddress()
{
    return m_individualAddress;
}

void GadObject::setIndividualAddress(eibaddr_t addr)
{
    m_individualAddress = addr;
}

void GadObject::destroy()
{
    while(!m_objects.empty())
    {
        GadObject *obj = m_objects.back();
        delete obj;
        m_objects.pop_back();
    }
}

void GadObject::fromBus(const std::vector<unsigned char> &data, ClientConnection *sender)
{
    /* KNX command */
    unsigned char cmd = static_cast<unsigned char>(((data[6] & 0x03) << 2) | ((data[7] & 0xC0) >> 6));

#if defined(DEBUG)
    std::cout << "[KNX] Bus notification " << cmd << " for " << GroupAddressToString(m_gad) << ": ";
    dump_telegram(data);
    std::cout << std::endl;
#endif

    switch(cmd)
    {
    case KNX_RESPONSE:
    case KNX_WRITE:
        /* Change cache value */
        m_data = std::vector<unsigned char>(data.begin() + 7, data.end() - 1);
        m_data[0] &= 0x3F;
        m_lastupdate = std::chrono::system_clock::now();
        m_valid = true;
    }

#if defined(DEBUG)
    std::cout << "    -> data: ";
    dump_telegram(m_data);
    std::cout << std::endl;
#endif

    for(ClientConnection *client: m_subscriber)
    {
        if(client == sender)
            continue;
        client->write(data);
    }
    for(ClientConnection *client: m_dmz)
    {
        if(client == sender)
            continue;
        client->write(data);
    }
}

#if 0
void GadObject::sendRead()
{
    std::cout << "[KNX] Send Read: " << GroupAddressToString(m_gad) << std::endl;
    const uint8_t message[] = {0x00, 0x00};
    EIBSendGroup(m_knxd, m_gad, 2, message);
    read(0);
}

void GadObject::sendWrite(const std::vector<unsigned char> &buffer)
{
    unsigned char cmd = buffer[1] & 0xC0;
    std::cout << "[KNX] Send " << std::string((cmd == 0x80)?("Write:"):((cmd == 0x40)?("Response:"):("?????:"))) << GroupAddressToString(m_gad) << "\t";
    for(unsigned char c: buffer)
        std::cout << std::setfill('0') << std::setw(2) << std::hex << int(c) << std::dec << ":";
    std::cout << "\b " << std::endl;

    EIBSendGroup(m_knxd, m_gad, buffer.size(), buffer.data());

    std::vector<unsigned char> data(buffer.data() + 1, buffer.data() + buffer.size());
    data[0] &= ~0xC0;
    if(cmd == 0x80)
    {
        write(0, data);
    }
    else if(cmd == 0x40)
    {
        response(0, data);
    }
}
#endif

void GadObject::sendCacheValueToClient(ClientConnection *client)
{
    std::vector<unsigned char> message;
    message.resize(8);
    message[0] = TELEGRAM_BEGIN;
    message[1] = static_cast<unsigned char>(m_data.size() + 8);
    message[2] = (m_individualAddress >> 8) & 0xFF;
    message[3] = m_individualAddress & 0xFF;
    message[4] = (m_gad >> 8) & 0xFF;
    message[5] = m_gad & 0xFF;
    message[6] = (KNX_CACHE_VALUE >> 2) & 0x37;
    message.insert(message.begin() + 7, m_data.begin(), m_data.end());
    message[7] = (message[7] & 0x37) | (KNX_CACHE_VALUE & 0x3) << 6;
    message[7 + m_data.size()] = TELEGRAM_END;
#if defined(DEBUG)
    std::cout << "[KNX] " << GroupAddressToString(m_gad) << " send cache value to " << client->sd() << " (len: " << message.size() << ")" <<  std::endl;
#endif
    client->write(message);
}

void GadObject::removeAll(ClientConnection *client)
{
    m_subscriber.erase(std::remove(m_subscriber.begin(), m_subscriber.end(), client), m_subscriber.end());
}

void GadObject::subscribe(ClientConnection *client)
{
    if(std::find(m_subscriber.begin(), m_subscriber.end(), client) == m_subscriber.end())
    {
        m_subscriber.push_back(client);
        if(m_valid)
        {
            sendCacheValueToClient(client);
        }
        else
        {
            /* TODO: Send READ to bus */
        }
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
    if(std::find(m_dmz.begin(), m_dmz.end(), client) == m_dmz.end())
    {
        m_dmz.push_back(client);
    }
}

void GadObject::dmzUnsubscribe(ClientConnection *client)
{
    m_dmz.erase(std::remove(m_dmz.begin(), m_dmz.end(), client), m_dmz.end());
}

void GadObject::setKnxd(EIBConnection *knxd)
{
    m_knxd = knxd;
}

