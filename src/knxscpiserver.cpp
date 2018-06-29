#include "knxscpiserver.h"

#include <string>
#include <iostream>
#include <cstdio>
#include <cstring>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "common.h"
#include "knxscpiclient.h"

using namespace std;


KnxObjectPool *KnxScpiServer::pool() const
{
    return _pool;
}

KnxScpiServer::KnxScpiServer(KnxObjectPool *pool):
    _pool(pool),
    _shutdown(false)
{

}

KnxScpiServer::~KnxScpiServer()
{
    while(_clients.size())
    {
        KnxScpiClient *cl = _clients.at(0);
        log_time();
        cout << "Need close client" <<endl;
        cl->stop();
        cl->join();
        _clients.erase(_clients.begin());
        delete cl;
    }
}

bool KnxScpiServer::shutdown() const
{
    return _shutdown;
}

void KnxScpiServer::start()
{
    _threadServer = std::thread(
        [this]()
        {
            /* Create socket */
            int serverFd = socket(AF_INET , SOCK_STREAM , 0);
            if(serverFd == -1)
            {
                cerr << "Could not create socket" << endl;
                return;
            }
            int istrue = 1;
            setsockopt(serverFd,SOL_SOCKET,SO_REUSEADDR,&istrue,sizeof(int));

            /* Prepare the sockaddr_in structure */
            struct sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_addr.s_addr = INADDR_ANY;
            server.sin_port = htons( 6721 );

            /* Bind socket */
            if( bind(serverFd, reinterpret_cast<struct sockaddr *>(&server) , sizeof(server)) < 0)
            {
                perror("Could not bind socket. Error");
                return;
            }

            /* Start Listen */
            listen(serverFd , 3);
            log_time();
            cout << "Server listen on port " << ntohs(server.sin_port) << endl;

            while(!shutdown())
            {
                int res;
                fd_set readset;
                struct timeval tv;
                FD_ZERO(&readset);
                FD_SET(serverFd, &readset);
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                res = select(serverFd+1, &readset, nullptr, nullptr, &tv);
                if(res == 0) // timeout
                {
                    checkClients();
                    continue;
                }
                createClient(serverFd);
            }

            for(uint32_t i=0; i<10; i++)
            {
                int n = close(serverFd);
                if(!n) {
                    log_time();
                    cout << "Server closed" << endl;
                    return;
                }
                usleep(100);
            }
            cout << "Server fail to close" << endl;
        }
    );
}

void KnxScpiServer::stop()
{
    for(KnxScpiClient *client : _clients)
    {
        client->stop();
    }
    _shutdown = true;
}

void KnxScpiServer::join()
{
    for(KnxScpiClient *client : _clients)
    {
        client->join();
    }
    _threadServer.join();
}

void KnxScpiServer::checkClients()
{
    bool loop = true;
    unsigned int i = 0;

    while(loop)
    {
        bool findone = false;
        loop = false;
        unsigned int todel = 0;
        for(auto client = _clients.begin(); client != _clients.end(); ++client)
        {
            if((*client)->closed())
            {
                findone = true;
                todel = i;
                break;
            }
            ++i;
        }
        if(findone)
        {
            KnxScpiClient *cl = _clients.at(todel);
            log_time();
            cl->join();
            _clients.erase(_clients.begin() + todel);
            delete cl;
            loop = true;
        }
    }
}

KnxScpiClient *KnxScpiServer::createClient(int serverFd)
{
    KnxScpiClient *client = new KnxScpiClient(this, serverFd);
    _clients.push_back(client);
    return client;
}
