#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <memory>
#include <iomanip>
#include <ctime>
#include <string>
#include <fstream>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "eibclient.h"
#include "clientconnection.h"
#include "clientconnectionssl.h"
#include "gadobject.h"

#if defined (WITH_SSL_SOCKET)
    SSL_CTX *ssl_ctx = nullptr;
#endif

static std::map<std::string, std::string> server_configuration;

#define DEBUG_QUERY
//#define DEBUG_DECODE_KNX

/*
 * Message Format for command:
 *   FLC...
 *
 * F 1 byte: 0xFF (Binary command Type 0)
 * L 1 byte: Len of parameters
 * C 1 byte: Command
 *
 * Command:
 *  01: Get all cached values
 *  02: Subscribe, need 1 parameter 2byte: eibaddr_t
 *  03: Unsubscribe, need 1 parameter 2byte: eibaddr_t
 *  04: Add to DMZ list (recive all events)
 *  05: Remove to DMZ list
 */



static bool running = true;
static int server_socket = 0;

#if defined(WITH_SSL_SOCKET)
static int server_socket_ssl = 0;
#endif


void sig_handler(int sig)
{
    if(sig == SIGTERM || sig == SIGINT)
    {
        std::cout << "Ask Shutdown" << std::endl;
        if(server_socket)
            close(server_socket);
        running = false;
    }
}

int create_socket(int port, const char* name)
{
    int sock = 0;
    int opt = 1;
    struct sockaddr_in address;

    if( (sock = socket(AF_INET , SOCK_STREAM , 0)) == 0)
    {
        std::cerr << "socket failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0 )
    {
        std::cerr << "setsockopt" << std::endl;
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons( port );

    //bind the socket to localhost port 6721
    if (bind(sock, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0)
    {
        std::cerr << "bind failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "[server] Listener " << name << "on port " << port << std::endl;

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(sock, 3) < 0)
    {
        std::cerr << "listen" << std::endl;
        exit(EXIT_FAILURE);
    }

    return sock;
}

#if defined (WITH_SSL_SOCKET)
void init_openssl()
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl()
{
    EVP_cleanup();
}

int pem_passwd_cb(char *buf, int size, int rwflag, void *userdata)
{
    strncpy(buf, server_configuration["ssl_private_key_password"].c_str(), size);
    buf[size - 1] = '\0';
    return strlen(buf);
}

SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    SSL_CTX_set_default_passwd_cb(ctx, pem_passwd_cb);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);

    if(
            !SSL_CTX_use_certificate_file(ctx, server_configuration["ssl_cert_file"].c_str(), SSL_FILETYPE_PEM)
         || !SSL_CTX_use_PrivateKey_file(ctx, server_configuration["ssl_private_key_file"].c_str(), SSL_FILETYPE_PEM)
         || !SSL_CTX_check_private_key(ctx)) {
      ERR_print_errors_fp(stderr);
      std::cerr << "Error setting up SSL_CTX." << std::endl;
      return nullptr;
    }

    if(!SSL_CTX_load_verify_locations(ctx, server_configuration["ssl_ca_file"].c_str(), nullptr))
    {
        std::cerr << "Could not load trusted CA certificates." << std::endl;
        return nullptr;
    }
    return ctx;
}
#endif

std::vector<std::string> split(const std::string& s, char seperator)
{
   std::vector<std::string> output;

    std::string::size_type prev_pos = 0, pos = 0;

    while((pos = s.find(seperator, pos)) != std::string::npos)
    {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );

        output.push_back(substring);

        prev_pos = ++pos;
    }

    output.push_back(s.substr(prev_pos, pos-prev_pos)); // Last word

    return output;
}

int main(int argc, char** argv)
{
    unsigned char buffer[1025];  //data buffer of 1K
    ssize_t valread = 0;
    int addrlen = 0;
    int len = 0;
    int knxd_socket = 0;
    fd_set readfds;
    int max_sd = 0;
    int activity = 0;
    struct sockaddr_in address{};
    eibaddr_t dest;
    eibaddr_t src;
    struct sigaction sa = {};
    EIBConnection *knxd = nullptr;

    std::ifstream infile("/etc/knxcached.conf");
    std::string line;
    while (std::getline(infile, line))
    {
        if(line[0] == '#')
            continue;
        line.erase(line.find_last_not_of(" \n\r\t")+1);

        std::vector<std::string> lineconf = split(line, '=');
        if(lineconf.size() != 2)
            continue;
        server_configuration[lineconf[0]] = lineconf[1];
    }

    running = true;
    sa.sa_handler = sig_handler;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    knxd = EIBSocketURL(server_configuration["knxd_url"].c_str());
    if (EIBOpen_GroupSocket (knxd, 0) == -1)
    {
        std::cerr << "Error opening knxd socket (" << server_configuration["knxd_url"] << ")" << std::endl;
        exit(1);
    }
    GadObject::setKnxd(knxd);
    knxd_socket = EIB_Poll_FD(knxd);


#if defined (WITH_SSL_SOCKET)
    SSL_CTX *ctx = nullptr;
    if(server_configuration.count("ssl_server_port"))
    {
        init_openssl();
        ctx = create_context();
        if(ctx)
        {
            server_socket_ssl = create_socket(std::stoi(server_configuration["ssl_server_port"]), "ssl ");
        }
    }
#endif
    server_socket = create_socket(std::stoi(server_configuration["server_port"]), "");


    /* EVENT LOOP */
    while(running)
    {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket and knx socket to set
        FD_SET(knxd_socket, &readfds);
#if defined (WITH_SSL_SOCKET)
        if(ctx)
        {
            FD_SET(server_socket_ssl, &readfds);
        }
#endif
        FD_SET(server_socket, &readfds);
        max_sd = server_socket;

        for(int sd: ClientConnection::fds())
        {
            if(sd > 0)
            {
                FD_SET(sd, &readfds);

                //highest file descriptor number, need it for the select function
                if(sd > max_sd)
                    max_sd = sd;
            }
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds, nullptr, nullptr, nullptr);
        if(!running)
            break;

        if ((activity < 0) && (errno != EINTR))
        {
            std::cerr << "select error" << std::endl;
        }

        //If something happened on the master socket , then its an incoming connection
#if defined (WITH_SSL_SOCKET)
        if (FD_ISSET(server_socket_ssl, &readfds))
        {
            SSL *ssl;
            int new_socket;
            addrlen = 0;

            if ((new_socket = accept(
                     server_socket_ssl,
                     reinterpret_cast<struct sockaddr *>(&address),
                     reinterpret_cast<socklen_t*>(&addrlen)
                     ) ) < 0)
            {
                std::cerr << "Accept error " << new_socket  << " (" << errno << ")" << std::endl;
                continue;
            }
            new ClientConnectionSsl(new_socket, &address);
        }
#endif

        if (FD_ISSET(server_socket, &readfds))
        {
            int new_socket;
            addrlen = 0;

            if ((new_socket = accept(
                     server_socket,
                     reinterpret_cast<struct sockaddr *>(&address),
                     reinterpret_cast<socklen_t*>(&addrlen)
                     ) ) < 0)
            {
                std::cerr << "Accept error " << new_socket  << " (" << errno << ")" << std::endl;
                continue;
            }

            //inform user of socket number - used in send and receive commands
            new ClientConnection(new_socket, &address);
        }

        if (FD_ISSET(knxd_socket, &readfds))
        {
            len = EIBGetGroup_Src (knxd, sizeof (buffer), buffer, &src, &dest);
            std::vector<unsigned char> data(buffer + 1, buffer + len);
            data[0] &= ~0xC0;
            unsigned char command = static_cast<unsigned char>(((buffer[1] & 0xC0) >> 6) | ((buffer[0] & 0x3)  << 2));
            GadObject *object = GadObject::getObject(dest);
            if(object)
            {
                switch(command)
                {
                    case NOTIFY_KNX_READ:
                        object->read(src);
                        break;

                    case NOTIFY_KNX_RESPONSE:
                        object->response(src, data);
                        break;

                    case NOTIFY_KNX_WRITE:
                        object->write(src, data);
                        break;

                    case NOTIFY_KNX_MEMWRITE:
                        object->memwrite(src, data);
                        break;

                    default:
                        std::cerr << "[KNX] Unknown Command " << command << " [" << len << "]" << std::endl;
                }
            }
        }

        //else its some IO operation on Client Connection :)
        for (int sd: ClientConnection::fds())
        {
            if (FD_ISSET( sd, &readfds))
            {
                //Check if it was for closing , and also read the incoming message
                ClientConnection *client = ClientConnection::getConnection(sd);
                if(client)
                {
                    if(!client->incomingData())
                    {
                        delete client;
                    }
                }
            }
        }
    } /* Main Loop */

    EIBClose(knxd);
    if(server_socket)
        close(server_socket);

#if defined (WITH_SSL_SOCKET)
    if(server_socket_ssl)
    {
        close(server_socket);
        SSL_CTX_free(ctx);
        cleanup_openssl();
    }
#endif

    return 0;
}
