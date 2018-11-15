#include "clientconnection.h"
#include "gadobject.h"
#include "command_type.h"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEBUG

std::vector<ClientConnection *> ClientConnection::m_connections;
std::vector<ClientConnection *> ClientConnection::m_dmz;

ClientConnection::ClientConnection(int sd, struct sockaddr_in *address, bool native)
    : m_sd(sd)
{
    m_address = *address;
    if(native)
    {
        addConnection(this);
        write(banner);
        printf("[%i] Connection %s:%d\n" , sd, inet_ntoa(m_address.sin_addr), ntohs(m_address.sin_port));
    }
}

ClientConnection::~ClientConnection()
{
    close( m_sd );
    m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), this), m_connections.end());
    GadObject::forgotClient(this);
    if(m_msgdisconnect)
    {
        printf("[%i] Disconnection %s:%d\n" , m_sd, inet_ntoa(m_address.sin_addr) , ntohs(m_address.sin_port));
        m_msgdisconnect = false;
    }
}

std::vector<int> ClientConnection::fds()
{
    std::vector<int> list;
    list.reserve(m_connections.size());
    for(ClientConnection* client: m_connections)
    {
        list.push_back(client->sd());
    }
    return list;
}

void ClientConnection::addConnection(ClientConnection *me)
{
    m_connections.push_back(me);
}

ClientConnection *ClientConnection::getConnection(int fd)
{
    for(ClientConnection *client: m_connections)
        if(client->sd() == fd)
            return client;
    return nullptr;
}

int ClientConnection::sd() const
{
    return m_sd;
}

bool ClientConnection::incomingData()
{
    static unsigned char buffer[4096];
    static unsigned char carry_buffer[512];
    static size_t carry = 0;

    ssize_t start = 0;
    if(carry)
    {
        start = static_cast<ssize_t>(carry);
        memcpy(buffer, carry_buffer, carry);
        carry = 0;
    }

    ssize_t ret = this->read(buffer + start, static_cast<size_t>(4096 - start));
    if(ret == 0)
    {
        /* Socket disconnection */
        return false;
    }

    start = 0;
    while(start < ret)
    {
        if(buffer[start] != TELEGRAM_BEGIN)
        {
            std::cerr << "INVALID TELEGRAM BEGIN, forget all following" << std::endl;
            carry = 0;
            return true;
        }

        if(buffer[start + 1] > (4096 - start))
        {
            /* Incomplet telegram, add to it carry */
            if(4096 - start > 512)
            {
                std::cerr << "INVALID TELEGRAM CARRY, forget all following" << std::endl;
                carry = 0;
                return true;
            }
            memcpy(carry_buffer, buffer + start, 4096 - start);
            carry = 4096 - start;
            return true;
        }

        if(start + buffer[start + 1] > 4096)
        {
            std::cerr << "INVALID TELEGRAM LENGHT, forget all following" << std::endl;
            carry = 0;
            return true;
        }

        if(buffer[start + buffer[start + 1] - 1] != TELEGRAM_END)
        {
            std::cerr << "INVALID TELEGRAM END, forget all following" << std::endl;
            carry = 0;
            return true;
        }

        /* We can process telegram between [start] and [start + buffer[start + 1]] */
        unsigned char* first = buffer + start;
        unsigned char* last = buffer + start + buffer[start + 1] - 1;
        std::vector<unsigned char> data(first, last);
        unsigned char cmd = static_cast<unsigned char>(((data[6] & 0x37) << 2) | ((data[7] & 0xC0) >> 6));
        eibaddr_t src = static_cast<eibaddr_t>(data[2] << 8 | data[3]);
        eibaddr_t dest = static_cast<eibaddr_t>(data[4] << 8 | data[5]);
        if(src && src != m_knxaddress)
        {
            m_knxaddress = src;
#if defined(DEBUG)
            std::cout << "[" << m_sd << "] Set Individual address to "
                      << ((m_knxaddress >> 12) & 0x0F)
                      << "."
                      << ((m_knxaddress >> 8) & 0x0F)
                      << "."
                      << (m_knxaddress & 0xFF)
                      << std::endl;
#endif
        }

        switch(cmd)
        {
            case CMD_DUMP_CACHED:
            {
#if defined(DEBUG)
                std::cout << "[" << m_sd << "] CMD_DUMP_CACHED" << std::endl;
#endif
                for(GadObject *obj: GadObject::objects())
                    obj->sendCacheValueToClient(this);
                sendEof();
                break;
            }
            case CMD_SUBSCRIBE:
            {
#if defined(DEBUG)
                std::cout << "[" << m_sd << "] CMD_SUBSCRIBE " << GroupAddressToString(dest) << std::endl;
#endif
                GadObject *obj = GadObject::getObject(dest);
                ClientConnection *client = ClientConnection::getConnection(m_sd);
                if(obj && client)
                {
                    obj->subscribe(client);
                }
                break;
            }
            case CMD_UNSUBSCRIBE:
            {
#if defined(DEBUG)
                std::cout << "[" << m_sd << "] CMD_UNSUBSCRIBE " << GroupAddressToString(dest) << std::endl;
#endif
                GadObject *obj = GadObject::getObject(dest);
                ClientConnection *client = ClientConnection::getConnection(m_sd);
                if(obj && client)
                {
                    obj->unsubscribe(client);
                }
                break;
            }
            case CMD_SUBSCRIBE_DMZ:
            {
#if defined(DEBUG)
                std::cout << "[" << m_sd << "] CMD_SUBSCRIBE_DMZ" << std::endl;
#endif
                ClientConnection *client = ClientConnection::getConnection(m_sd);
                GadObject::dmzSubscribe(client);
                break;
            }
            case CMD_UNSUBSCRIBE_DMZ:
            {
#if defined(DEBUG)
                std::cout << "[" << m_sd << "] CMD_UNSUBSCRIBE_DMZ" << std::endl;
#endif
                ClientConnection *client = ClientConnection::getConnection(m_sd);
                GadObject::dmzUnsubscribe(client);
                break;
            }
            case KNX_WRITE:
            case KNX_RESPONSE:
            case KNX_READ:
            {
                GadObject *obj = GadObject::getObject(dest);
                if(obj)
                {
                    std::vector<unsigned char> kdata(data.begin() + 6, data.end());
#if defined(DEBUG)
                    std::cout << "[" << m_sd << "] SEND KNX TELEGRAM";
                    dump_telegram(kdata);
                    std::cout << std::endl;
#endif
                    obj->sendToBus(kdata, this);
                }
            }
        }

        /* Next one */
        start += buffer[start + 1];
    }
    return true;
}

ssize_t ClientConnection::write(const std::vector<unsigned char> &data)
{
#if defined(DEBUG)
    if(data[0] != 'K')
    {
        std::cout << "[" << m_sd << "] Write : ";
        dump_telegram(data);
        std::cout << std::endl;
    }
#endif
    return send(m_sd, data.data(), data.size(), 0);
}

ssize_t ClientConnection::read(void *buf, size_t nbytes)
{
    return recv(m_sd, buf, nbytes, 0);
}

void ClientConnection::sendEof()
{
    static std::vector<unsigned char> eof = {TELEGRAM_BEGIN, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, TELEGRAM_END};
    if(eof[1] == 0)
    {
        eibaddr_t src = GadObject::IndividualAddress();
        eof[1] = eof.size();
        eof[2] = (src >> 8) & 0xFF;
        eof[3] = src & 0xFF;
        eof[6] = TELEGRAM_EOT >> 2;
        eof[7] = (TELEGRAM_EOT & 0x3) << 6;
    }
    write(eof);
}

eibaddr_t ClientConnection::knxaddress()
{
    return m_knxaddress;
}
