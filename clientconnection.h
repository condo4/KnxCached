#ifndef CLIENTCONNECTION_H
#define CLIENTCONNECTION_H

#include <vector>
#include <sys/types.h>


#define CMD_DUMP_CACHED     (0x01)
#define CMD_SUBSCRIBE       (0x02)
#define CMD_UNSUBSCRIBE     (0x03)
#define CMD_SUBSCRIBE_DMZ   (0x04)
#define CMD_UNSUBSCRIBE_DMZ (0x05)
#define CMD_REQUEST_VALUE   (0x06)
#define CMD_SEND_WRITE      (0x07)
#define CMD_READ            (0x08)
#define CMD_SETDEBUG        (0x09)



static const std::vector<unsigned char> banner = {'K','n','x','C','a','c','h','e','d',':','2','.','0'};

class ClientConnection
{
protected:
    int m_sd;
    bool m_debug {false};
    bool m_msgdisconnect {true};
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
