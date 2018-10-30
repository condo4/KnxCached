#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
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

#if defined(WITH_SSL_SOCKET)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif


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

#define CMD_DUMP_CACHED     (0x01)
#define CMD_SUBSCRIBE       (0x02)
#define CMD_UNSUBSCRIBE     (0x03)
#define CMD_SUBSCRIBE_DMZ   (0x04)
#define CMD_UNSUBSCRIBE_DMZ (0x05)
#define CMD_REQUEST_VALUE   (0x06)
#define CMD_SET_VALUE       (0x07)

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

#define NOTIFY_KNX_READ     (0b00000000)
#define NOTIFY_KNX_RESPONSE (0b00000001)
#define NOTIFY_KNX_WRITE    (0b00000010)
#define NOTIFY_KNX_MEMWRITE (0b00001010)
#define NOTIFY_EOT          (0b10000001)

static std::map<std::string, std::string> server_configuration;

static bool running = true;
static int server_socket = 0;
static std::vector<int> clients;
static std::vector<int> dmz;
#if defined(WITH_SSL_SOCKET)
static int server_socket_ssl = 0;
static std::map<int, SSL *> ssl_Socks;
#endif
static std::map<eibaddr_t, std::vector<unsigned char>> data_cache;
static std::map<eibaddr_t, std::vector<int>> subscriber;

std::string GroupAddressToString(unsigned short addr)
{
    char buff[10];
    snprintf (buff, 10, "%d/%d/%d", (addr >> 11) & 0x1f, (addr >> 8) & 0x07, (addr) & 0xff);
    buff[9] = '\0';
    return std::string(buff);
}

inline ssize_t sock_send(int sd, const void *buf, size_t n, int flags)
{
#if defined (WITH_SSL_SOCKET)
    if(ssl_Socks.count(sd)) {
        // SSL Socket
        return SSL_write(ssl_Socks[sd], buf, static_cast<int>(n));
    } else {
#endif
    return send(sd, buf, n, flags);
#if defined (WITH_SSL_SOCKET)
    }
#endif
}

void notify_client(unsigned char command, eibaddr_t src, eibaddr_t addr)
{
    std::vector<unsigned char> message;
    message.push_back(0xFE);
    message.push_back(0x00); /* LEN TO SET AT THE END */
    message.push_back(command);
    message.push_back((src >> 8) & 0xFF);
    message.push_back(src & 0xFF);
    message.push_back((addr >> 8) & 0xFF);
    message.push_back(addr & 0xFF);

    if(command == NOTIFY_KNX_RESPONSE || command == NOTIFY_KNX_WRITE)
    {
        for(unsigned char c:  data_cache[addr])
            message.push_back(c);
    }
    message[1] = static_cast<unsigned char>(message.size() - 3);

    if((command & 0xF0) == 0x00)
    {
        /* KNX command */
        for(int &sd: subscriber[addr])
        {
            sock_send(sd, message.data(), message.size(), 0);
        }

        for(int &sd: dmz)
        {
            if(sock_send(sd, message.data(), message.size(), 0) < static_cast<ssize_t>(message.size()))
            {
                std::cerr << "send error! " << std::endl;
            }
        }
    }
    else
        std::cout << "? COMMAND" << (command & 0xF0) << std::endl;
}


void send_value_to_client(int sd, eibaddr_t addr)
{
    std::vector<unsigned char> message;
    message.push_back(0xFE);
    message.push_back(0x00); /* LEN TO SET AT THE END */
    message.push_back(NOTIFY_KNX_RESPONSE);
    message.push_back(0);
    message.push_back(0);
    message.push_back((addr >> 8) & 0xFF);
    message.push_back(addr & 0xFF);
    for(unsigned char c:  data_cache[addr])
        message.push_back(c);
    message[1] = static_cast<unsigned char>(message.size() - 3);
    sock_send(sd, message.data(), message.size(), 0);
}

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
    const char *message = "KnxCached:2.0";
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

        for(int &sd: clients)
        {
            if(sd > 0)
            {
                FD_SET( sd , &readfds);

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

            ssl = SSL_new(ctx);
            SSL_set_fd(ssl, new_socket);

            if (SSL_accept(ssl) <= 0) {
                std::cerr << "SSL Accept failed" << std::endl;
                ERR_print_errors_fp(stderr);
                SSL_free(ssl_Socks[new_socket]);
                ssl_Socks.erase(new_socket);
            }
            else
            {

                ssl_Socks[new_socket] = ssl;
                printf("[%i] SSL Connection %s:%d\n" , new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                //send new connection greeting message
                if( sock_send(new_socket, message, strlen(message), 0) != static_cast<ssize_t>(strlen(message)))
                {
                    std::cerr << "send error hello" << std::endl;
                }
                else
                {
                    //add new socket to array of sockets
                    clients.push_back(new_socket);
                }
            }
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
            printf("[%i] Connection %s:%d\n" , new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            //send new connection greeting message
            if( sock_send(new_socket, message, strlen(message), 0) != static_cast<ssize_t>(strlen(message)))
            {
                std::cerr << "send error hello" << std::endl;
            }
            //add new socket to array of sockets
            clients.push_back(new_socket);
        }

        if (FD_ISSET(knxd_socket, &readfds))
        {
            EIB_Poll_Complete (knxd);
            len = EIBGetGroup_Src (knxd, sizeof (buffer), buffer, &src, &dest);
            std::vector<unsigned char> data(buffer + 1, buffer + len);
            data[0] &= ~0xC0;
            unsigned char command = static_cast<unsigned char>(((buffer[1] & 0xC0) >> 6) | ((buffer[0] & 0x3)  << 2));

            switch(command)
            {
                case NOTIFY_KNX_READ:
#if defined(DEBUG_DECODE_KNX)
                    std::cout << "[KNX] Read[" << len << "]:" << GroupAddressToString(dest) << std::endl;
#endif
                    notify_client(command, src, dest);
                    // TODO:
                    break;
                case NOTIFY_KNX_RESPONSE:
#if defined(DEBUG_DECODE_KNX)
                    std::cout << "[KNX] Response[" << len << "]:" << GroupAddressToString(dest) << std::endl;
#endif
                    if(data_cache.count(dest) == 0 || data_cache[dest] != data)
                    {
                        data_cache[dest] = data;
                        notify_client(command, src, dest);
                    }
                    break;
                case NOTIFY_KNX_WRITE:
#if defined(DEBUG_DECODE_KNX)
                    std::cout << "[KNX] Write[" << len << "]: " << GroupAddressToString(dest) << "\t";
                    for(unsigned char c:  data_cache[dest])
                        std::cout << std::setfill('0') << std::setw(2) << std::hex << int(c) << std::dec << ":";
                    std::cout << "\b > ";
                    for(unsigned char c:  data)
                        std::cout << std::setfill('0') << std::setw(2) << std::hex << int(c) << std::dec << ":";
                    std::cout << "\b" << std::endl;
#endif
                    if(data_cache.count(dest) == 0 || data_cache[dest] != data)
                    {
                        data_cache[dest] = data;
                        notify_client(command, src, dest);
                    }
                    break;
                case NOTIFY_KNX_MEMWRITE:
                    notify_client(command, src, dest);
#if defined(DEBUG_DECODE_KNX)
                    std::cout << "[KNX] MemoryWrite[" << len << "]:" << GroupAddressToString(dest) << std::endl;
#endif
                    break;
                default:
                    std::cerr << "[KNX] Unknown Command " << command << " [" << len << "]" << std::endl;
            }
        }

        //else its some IO operation on some other socket :)
        for (int &sd: clients)
        {
            if (FD_ISSET( sd , &readfds))
            {
                //Check if it was for closing , and also read the incoming message
#if defined(WITH_SSL_SOCKET)
                if(ssl_Socks.count(sd))
                {
                    valread = SSL_read(ssl_Socks[sd], buffer, 1024);
                }
                else
                {
#endif
                valread = read( sd , buffer, 1024);
#if defined(WITH_SSL_SOCKET)
                }
#endif

                if (valread == 0)
                {
                    //Somebody disconnected , get his details and print                   
#if defined(WITH_SSL_SOCKET)
                    if(ssl_Socks.count(sd))
                    {
                        SSL_free(ssl_Socks[sd]);
                        ssl_Socks.erase(sd);
                        getpeername(sd , reinterpret_cast<struct sockaddr*>(&address), reinterpret_cast<socklen_t*>(&addrlen));
                        printf("[%i] SSL Disconnection %s:%d\n" , sd, inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                    }
                    else
                    {
#endif
                        getpeername(sd , reinterpret_cast<struct sockaddr*>(&address), reinterpret_cast<socklen_t*>(&addrlen));
                        printf("[%i] Disconnection %s:%d\n", sd, inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
#if defined(WITH_SSL_SOCKET)
                    }
#endif

                    //Close the socket and mark as 0 in list for reuse
                    dmz.erase(std::remove(dmz.begin(), dmz.end(), sd), dmz.end());
                    for(auto &i: subscriber)
                        i.second.erase(std::remove(i.second.begin(), i.second.end(), sd), i.second.end());

                    close( sd );
                    sd = 0;
                    continue;
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
#if defined(DEBUG_QUERY)
                            std::cout << "[" << sd << "]\t CMD_DUMP_CACHED" << std::endl;
#endif
                            for(auto addr: data_cache)
                                send_value_to_client(sd, addr.first);
                            /* Send eof */
                            message.clear();
                            message.push_back(0xFE);
                            message.push_back(0x00); /* LEN TO SET AT THE END */
                            message.push_back(NOTIFY_EOT);
                            sock_send(sd, message.data(), message.size(), 0);
                            break;
                        case CMD_SUBSCRIBE:
                            if(len != 2)
                                break;
                            addr = static_cast<eibaddr_t>(buf[3] << 8 | buf[4]);
#if defined(DEBUG_QUERY)
                            std::cout << "[" << sd << "]\t CMD_SUBSCRIBE(" << GroupAddressToString(addr) << ")" << std::endl;
#endif
                            subscriber[addr].push_back(sd);
                            send_value_to_client(sd, addr);
                            break;
                        case CMD_UNSUBSCRIBE:
                            if(len != 2)
                                break;
                            addr = static_cast<eibaddr_t>(buf[3] << 8 | buf[4]);
#if defined(DEBUG_QUERY)
                            std::cout << "[" << sd << "]\t CMD_UNSUBSCRIBE(" << GroupAddressToString(addr) << ")" << std::endl;
#endif
                            subscriber[addr].erase(std::remove(subscriber[addr].begin(), subscriber[addr].end(), sd), subscriber[addr].end());
                            break;
                        case CMD_SUBSCRIBE_DMZ:
#if defined(DEBUG_QUERY)
                            std::cout << "[" << sd << "]\t CMD_SUBSCRIBE_DMZ" << std::endl;
#endif
                            dmz.push_back(sd);
                            break;
                        case CMD_UNSUBSCRIBE_DMZ:
#if defined(DEBUG_QUERY)
                            std::cout << "[" << sd << "]\t CMD_UNSUBSCRIBE_DMZ" << std::endl;
#endif

                            dmz.erase(std::remove(dmz.begin(), dmz.end(), sd), dmz.end());
                            break;

                        case CMD_REQUEST_VALUE:
                            addr = static_cast<eibaddr_t>(buf[3] << 8 | buf[4]);
#if defined(DEBUG_QUERY)
                            std::cout << "[" << sd << "]\t CMD_REQUEST_VALUE(" << GroupAddressToString(addr) << ")" << std::endl;
#endif
                            send_value_to_client(sd, addr);
                            /* Send eof */
                            message.clear();
                            message.push_back(0xFE);
                            message.push_back(0x00); /* LEN TO SET AT THE END */
                            message.push_back(NOTIFY_EOT);
                            sock_send(sd, message.data(), message.size(), 0);
                            break;
                        case CMD_SET_VALUE:
                            addr = static_cast<eibaddr_t>(buf[3] << 8 | buf[4]);
                            message.clear();
                            for(i = 0; i < buf[1] - 2; i++)
                            {
                                message.push_back(buf[i + 5]);
                            }
#if defined(DEBUG_QUERY)
                            std::cout << "[" << sd << "]\t CMD_SET_VALUE(" << GroupAddressToString(addr) << ") ";
#endif
                            for(unsigned char c: message)
                            {
                                std::cout << std::setw(2) << std::hex << int(c) << std::dec << ":";
                            }
                            std::cout << std::endl;
                            if(data_cache.count(addr) == 0 || data_cache[addr] != message)
                            {
                                data_cache[addr] = message;
                                notify_client(NOTIFY_KNX_WRITE, 0, addr);
                                EIBSendGroup(knxd, addr, static_cast<int>(message.size()), message.data());
                            }
                            break;
                        default:
                            std::cerr << "[" << sd << "]\t Received invalid command " << int(buf[2]) << std::endl;
                            break;
                        }
                    }
                    buf += 3 + buf[1];
                } while(buf != buffer + valread);
            }
        }
        clients.erase(std::remove(clients.begin(), clients.end(), 0), clients.end());

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

    for(int &sd: clients)
    {
        close(sd);
    }

    return 0;
}
