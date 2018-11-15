#ifndef CLIENTCONNECTION_H
#define CLIENTCONNECTION_H

#include <vector>
#include <sys/types.h>
#include <netinet/in.h>

static const std::vector<unsigned char> banner = {'K','n','x','C','a','c','h','e','d',':','2','.','0', '\0'};

class ClientConnection
{
protected:
    int m_sd;
    bool m_debug {true};
    bool m_msgdisconnect {true};
    struct sockaddr_in m_address {0,0, {static_cast<in_addr_t>(0xffffffff)}, {0}};
    static std::vector<ClientConnection *> m_connections;
    static std::vector<ClientConnection *> m_dmz;

public:
    ClientConnection(int sd, struct sockaddr_in *address, bool native=true);
    virtual ~ClientConnection();

    static std::vector<int> fds();
    static void addConnection(ClientConnection *me);
    static ClientConnection *getConnection(int fd);

    virtual ssize_t write(const std::vector<unsigned char> &data);
    virtual ssize_t read (void *buf, size_t nbytes);
    void sendEof();
    int sd() const;

    bool incomingData();
};

#endif // CLIENTCONNECTION_H
