#include "clientconnectionssl.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


extern SSL_CTX *ssl_ctx;

ClientConnectionSsl::ClientConnectionSsl(int sd, struct sockaddr_in *address)
    : ClientConnection(sd, address, false)
{
    m_ssl = SSL_new(ssl_ctx);
    SSL_set_fd(m_ssl, m_sd);

    if (SSL_accept(m_ssl) <= 0)
    {
        std::cerr << "SSL Accept failed" << std::endl;
        SSL_free(m_ssl);
    }
    else
    {
        addConnection(this);
        write(banner);
        printf("[%i] SSL Connection %s:%d\n" , sd, inet_ntoa(address->sin_addr), ntohs(address->sin_port));
    }
}

ClientConnectionSsl::~ClientConnectionSsl()
{
    SSL_free(m_ssl);
    if(m_msgdisconnect)
    {
        printf("[%i] SSL Disconnection %s:%d\n" , m_sd, inet_ntoa(m_address.sin_addr) , ntohs(m_address.sin_port));
        m_msgdisconnect = false;
    }
}

ssize_t ClientConnectionSsl::write(const std::vector<unsigned char> &data)
{
    return SSL_write(m_ssl, data.data(), data.size());
}

ssize_t ClientConnectionSsl::read(void *buf, size_t nbytes)
{
    return SSL_read(m_ssl, buf, nbytes);
}

bool ClientConnectionSsl::valid() const
{
    return m_valid;
}
