#ifndef CLIENTCONNECTIONSSL_H
#define CLIENTCONNECTIONSSL_H

#include "clientconnection.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

class ClientConnectionSsl : public ClientConnection
{
    SSL *m_ssl;
    bool m_valid {false};

public:
    ClientConnectionSsl(int sd, struct sockaddr_in *address);
    virtual ~ClientConnectionSsl();

    virtual ssize_t write(const std::vector<unsigned char> &data);
    virtual ssize_t read (void *buf, size_t nbytes);
    bool valid() const;
};

#endif // CLIENTCONNECTIONSSL_H
