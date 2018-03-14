#include "knxscpiclient.h"

#include <string>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>

#include "common.h"
#include "knxscpiserver.h"
#include "knxobjectpool.h"

using namespace std;

// trim from start (in place)
static inline void ltrim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](int ch) {
        return !isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(string &s) {
    s.erase(find_if(s.rbegin(), s.rend(), [](int ch) {
        return !isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(string &s) {
    ltrim(s);
    rtrim(s);
}


KnxScpiServer *KnxScpiClient::server() const
{
    return _server;
}

KnxScpiClient::KnxScpiClient(KnxScpiServer *server, int srvfd):
    _server(server),
    _shutdown(false),
    _closed(false),
    _mode(TextMode)
{
    int sockSize = sizeof(struct sockaddr_in);
    _clientFd = accept(srvfd, (struct sockaddr *)&_client, (socklen_t*)&sockSize);
    _listen.clear();
    _thread = thread([this]()
        {
            char client_message[2000];
            int read_size;
            string last;
            last.clear();

            log_time();
            cout << "Client " << _clientFd << " connected" << endl;
            while(!shutdown())
            {
                /* Read events */
                while(_events.dataAvailable())
                {
                    KnxDataChanged data;
                    _events.pop(data);
                    if(data.id.size() != 0)
                    {
                        if(_mode == TextMode)
                        {
                            stringstream wss;
                            wss << "@> " << data.id << ": " << data.text << " " << data.unity << endl;
                            write(_clientFd, wss.str().c_str() , wss.str().size());
                        }
                        else if (_mode == DateTextMode)
                        {
                            char tmp[28];
                            time_t t = time(0);   // get time now
                            struct tm * now = localtime( & t );
                            sprintf(tmp, "%i/%02i/%02i %02i:%02i:%02i: ",
                                   now->tm_year + 1900,
                                   now->tm_mon + 1,
                                   now->tm_mday,
                                   now->tm_hour,
                                   now->tm_min,
                                   now->tm_sec);
                            stringstream wss;
                            wss << "@> " << tmp << data.id << ": " << data.text << endl;
                            write(_clientFd, wss.str().c_str() , wss.str().size());
                        }
                        else if (_mode == B64Mode)
                        {
                            cout << "B64 MODE" << endl;
                        }
                        else {
                            cout << "BAD MODE" << endl;
                        }
                    }
                }

                /* Read input socket */
                int res;
                fd_set readset;
                struct timeval tv;
                FD_ZERO(&readset);
                FD_SET(_clientFd, &readset);
                tv.tv_sec = 0;
                tv.tv_usec = 100000;
                res = select(_clientFd+1, &readset, NULL, NULL, &tv);
                if(res == 0) // timeout
                    continue;
                memset(client_message, 0, 2000);
                read_size = recv(_clientFd, client_message , 2000 , 0);
                if(read_size == 0)
                {
                    break;
                }
                string data;
                data.clear();
                data = client_message;

                KnxObjectPool *pool = _server->pool();

                if(data[data.size() - 1] != '\n')
                {
                    cout << "INCOMPLETE FRAME [" << data << "]" << endl;
                    size_t n = data.rfind('\n');
                    if(n == string::npos)
                    {
                        last += data;
                        continue;
                    }
                    else
                    {
                        string newdata = last + data.substr(0, n);
                        last = data.substr(n + 1);
                        data = newdata;
                    }
                }
                else
                {
                    data = last + data;
                    if(last.size()) cout << "RE-COMPLETE FRAME [" << data << "]" << endl;
                    last.clear();
                }

                trim(data);
                istringstream f(data);
                string line;
                while(getline(f, line, '\n'))
                {

                    trim(line);
                    istringstream seq(line);
                    vector<string> cmd;
                    string s;

                    while (getline(seq, s, ' ')) {
                        trim(s);
                        cmd.push_back(s);
                    }

                    if(cmd[0] == "exit")
                    {
                        std::string msg("Exit asked\n");
                        write(_clientFd, msg.c_str() , msg.size());
                        _shutdown = true;
                        break;
                    }
                    else if(cmd[0] == "*idn?")
                    {
                        std::string msg("\"KnxCached\",0,0,1.0\n");
                        write(_clientFd, msg.c_str() , msg.size());
                        break;
                    }
                    else if(cmd[0] == "list?")
                    {
                        vector<const char *> ids;
                        pool->getObjIds(ids);
                        for(const char* id: ids)
                        {
                            const KnxObject *obj = pool->getObjById(id);
                            string gad = GroupAddressToString(obj->gad());
                            write(_clientFd, gad.c_str(), gad.size());
                            write(_clientFd, " " , 1);
                            write(_clientFd, id , strlen(id));
                            write(_clientFd, "\n" , 1);
                        }
                    }
                    else if(cmd[0] == "values?")
                    {
                        vector<const char *> ids;
                        pool->getObjIds(ids);
                        for(const char* id: ids)
                        {
                            const KnxObject *obj = pool->getObjById(id);
                            if(obj && obj->initialized())
                            {
                                string res = obj->id() + ": " + obj->value() + " " + obj->unity();
                                write(_clientFd, res.c_str() , res.size());
                                write(_clientFd, "\n" , 1);
                            }
                        }
                    }
                    else if(cmd[0] == "listen?")
                    {
                        for(auto obj: _listen)
                        {
                            write(_clientFd, obj->id().c_str(), obj->id().size());
                            write(_clientFd, "\n" , 1);
                        }
                    }
                    else if(cmd[0] == "mode" && cmd.size() >= 2)
                    {
                        if(cmd[1] == "datetext")
                        {
                            _mode = DateTextMode;
                        }
                        else if (cmd[1] == "b64")
                        {
                            _mode = B64Mode;
                        }
                        else
                        {
                            _mode = TextMode;
                        }
                    }
                    else if(cmd[0] == "listen"  && cmd.size() >= 2)
                    {
                        const KnxObject *obj = NULL;
                        string msg;
                        msg.clear();
                        if(cmd[1] == "*")
                        {
                            vector<const char *> ids;
                            pool->getObjIds(ids);
                            for(const char* id: ids)
                            {
                                const KnxObject *obj = pool->getObjById(id);
                                if(obj)
                                {
                                    obj->connect(&_events);
                                    _listen.push_back(obj);
                                }
                            }
                        }
                        else if(!isdigit(cmd[1].c_str()[0]))
                        {
                            obj = pool->getObjById(cmd[1]);
                        }
                        else if(cmd[1].find("/") != string::npos)
                        {
                            unsigned short gad = 0;
                            gad = StringToGroupAddress(cmd[1]);
                            obj = pool->getObjByGad(gad);
                        }
                        else
                        {
                            unsigned short gad = 0;
                            gad = stoi(cmd[1]);
                            obj = pool->getObjByGad(gad);
                        }
                        if(obj)
                        {
                            obj->connect(&_events);
                            _listen.push_back(obj);
                        }
                        else
                        {
                            if(cmd[1] != "*")
                            {
                                msg = "Can't find knx object for ";
                                msg.append(cmd[1]);
                                write(_clientFd, msg.c_str(), msg.size());
                                write(_clientFd, "\n" , 1);
                            }
                        }
                    }
                    else if(cmd[0] == "set" && cmd.size() >= 3)
                    {
                        KnxObject *obj = pool->getObjById(cmd[1]);
                        if(obj)
                        {
                            obj->setValue(cmd[2]);
                        }
                    }
                    else
                    {
                        string msg;
                        msg.clear();
                        msg = "?? #";
                        msg.append(data);
                        msg.append("# ??\n");
                        write(_clientFd, msg.c_str() , msg.size());
                    }
                }
            }

            for(auto obj: _listen)
            {
                obj->disconnect(&_events);
            }

            closeFd();
            log_time();
            cout << "Client " << _clientFd << " halted" << endl;
            return;
        }
    );
}

bool KnxScpiClient::closed() const
{
    return _closed;
}

bool KnxScpiClient::shutdown() const
{
    return _shutdown;
}

void KnxScpiClient::stop()
{
    std::string msg("Stoping server, disconnect\n");
    write(_clientFd, msg.c_str() , msg.size());
    _shutdown = true;
}

void KnxScpiClient::join()
{
    _thread.join();
}

void KnxScpiClient::closeFd()
{
    _closed = true;
    close(_clientFd);
}
