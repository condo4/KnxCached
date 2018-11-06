#include "clientconnection.h"
#include "gadobject.h"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


std::vector<ClientConnection *> ClientConnection::m_connections;
std::vector<ClientConnection *> ClientConnection::m_dmz;

ClientConnection::ClientConnection(int sd, struct sockaddr_in *address, bool native)
    : m_sd(sd)
{
    if(native)
    {
        addConnection(this);
        write(banner);
        printf("[%i] Connection %s:%d\n" , sd, inet_ntoa(address->sin_addr), ntohs(address->sin_port));
    }
}

ClientConnection::~ClientConnection()
{
    close( m_sd );
    m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), this), m_connections.end());
    GadObject::forgotClient(this);
    if(m_msgdisconnect)
    {
        struct sockaddr_in address;
        int addrlen;
        getpeername(m_sd, reinterpret_cast<struct sockaddr*>(&address), reinterpret_cast<socklen_t*>(&addrlen));
        printf("[%i] Disconnection %s:%d\n" , m_sd, inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
        m_msgdisconnect = false;
    }
}

std::vector<int> ClientConnection::fds()
{
    std::vector<int> list;
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
    static unsigned char buffer[4096];  //data buffer of 4K
    ssize_t valread = this->read(buffer, 4095);
    if (valread == 0)
    {
        return false;
    }
    unsigned char *buf = buffer;
    do
    {
        if(buf + 3 + buf[1] > buffer + valread)
        {
            std::cerr << "Incomplete buffer" << std::endl;
            break;
        }

        if(buf[0] == 0xFF)
        {
            unsigned char len = buf[1];
            eibaddr_t addr = 0;
            int i;
            std::vector<unsigned char> message;

            switch(buf[2])
            {
            case CMD_DUMP_CACHED:
            {
                if(m_debug)
                    std::cout << "[" << m_sd << "] CMD_DUMP_CACHED" << std::endl;
                for(GadObject *obj: GadObject::objects())
                    obj->sendResponse(this);
                /* Send eof */
                sendEof();
                break;
            }
            case CMD_SUBSCRIBE:
            {
                if(len != 2)
                    break;
                GadObject *obj = GadObject::getObject(static_cast<eibaddr_t>(buf[3] << 8 | buf[4]));
                if(m_debug)
                    std::cout << "[" << m_sd << "] CMD_SUBSCRIBE " << GroupAddressToString(obj->gad()) << std::endl;
                ClientConnection *client = ClientConnection::getConnection(m_sd);
                if(obj && client)
                {
                    obj->subscribe(client);
                }
                break;
            }
            case CMD_UNSUBSCRIBE:
            {
                if(len != 2)
                    break;
                GadObject *obj = GadObject::getObject(static_cast<eibaddr_t>(buf[3] << 8 | buf[4]));
                if(m_debug)
                    std::cout << "[" << m_sd << "] CMD_UNSUBSCRIBE " << GroupAddressToString(obj->gad()) << std::endl;
                ClientConnection *client = ClientConnection::getConnection(m_sd);
                if(obj && client)
                {
                    obj->unsubscribe(client);
                }
                break;
            }
            case CMD_SUBSCRIBE_DMZ:
            {
                if(m_debug)
                    std::cout << "[" << m_sd << "] CMD_SUBSCRIBE_DMZ " << std::endl;
                ClientConnection *client = ClientConnection::getConnection(m_sd);
                GadObject::dmzSubscribe(client);
                break;
            }
            case CMD_UNSUBSCRIBE_DMZ:
            {
                if(m_debug)
                    std::cout << "[" << m_sd << "] CMD_UNSUBSCRIBE_DMZ " << std::endl;
                ClientConnection *client = ClientConnection::getConnection(m_sd);
                GadObject::dmzUnsubscribe(client);
                break;
            }
            case CMD_REQUEST_VALUE:
            {
                GadObject *obj = GadObject::getObject(static_cast<eibaddr_t>(buf[3] << 8 | buf[4]));
                if(m_debug)
                    std::cout << "[" << m_sd << "] CMD_REQUEST_VALUE " << GroupAddressToString(obj->gad()) << std::endl;
                if(obj)
                {
                    obj->sendResponse(this);
                    sendEof();
                }
                break;
            }
            case CMD_SEND_WRITE:
            {
                GadObject *obj = GadObject::getObject(static_cast<eibaddr_t>(buf[3] << 8 | buf[4]));
                if(m_debug)
                {
                    std::cout << "[" << m_sd << "]\t CMD_SEND_WRITE(" << GroupAddressToString(addr) << ") [";
                    for(i = 0; i < buf[1] + 3; i++)
                    {
                        std::cout << std::setw(2) << std::hex << int(buf[i]) << std::dec << ":";
                    }
                    std::cout << "\b]" <<  std::endl;
                }
                message.clear();
                message.push_back(0);
                for(i = 0; i < buf[1] - 2; i++)
                {
                    message.push_back(buf[i + 5]);
                }
                message[1] |= 0x80; // WRITE
                obj->sendWrite(message);
                break;
            }
            case CMD_READ:
            {
                GadObject *obj = GadObject::getObject(static_cast<eibaddr_t>(buf[3] << 8 | buf[4]));
                if(m_debug)
                    std::cout << "[" << m_sd << "] CMD_READ " << GroupAddressToString(obj->gad()) << std::endl;
                if(obj)
                {
                    obj->sendRead();
                }
                break;
            }
            case CMD_SETDEBUG:
            {
                m_debug = int(buf[3]) == 1;
                std::cout << "[" << m_sd << "] CMD_SETDEBUG: " << int(buf[3]) << std::endl;
                break;
            }
            default:
                std::cerr << "[" << m_sd << "]\t Received invalid command " << int(buf[2]) << std::endl;
                break;
            }
        }
        buf += 3 + buf[1];
    } while(buf != buffer + valread);
    return true;
}

ssize_t ClientConnection::write(const std::vector<unsigned char> &data)
{
    return send(m_sd, data.data(), data.size(), 0);
}

ssize_t ClientConnection::read(void *buf, size_t nbytes)
{
    return recv(m_sd, buf, nbytes, 0);
}

void ClientConnection::sendEof()
{
    static std::vector<unsigned char> eof = {0xFE, 0x00, NOTIFY_EOT};
    write(eof);
}
