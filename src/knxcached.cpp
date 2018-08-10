#include <iostream>
#include <ctime>
#include <string>
#include <csignal>
#include <cstring>

#include <argp.h>
#include <unistd.h>

#include "common.h"
#include "knxobjectpool.h"
#include "knxscpiserver.h"
#include "knxeventfifo.h"
#include "knxdatachanged.h"

using namespace std;

const char *argp_program_version = "knxcached 1.0";
const char *argp_program_bug_address = "<fabien.proriol@saint-pal.com>";
static char doc[] = "knxcached -- an pbject proxy for knxd";
static char args_doc[] = "";


/* The options we understand. */
static struct argp_option options[] = {
    {"verbose",  'v', nullptr,      0,  "Produce verbose output", 0 },
    {"conf",     'c', "FILE", 0,  "Configuration file instead of /etc/knxcached.xml", 0},
    { nullptr, 0, nullptr, 0, nullptr, 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
    int verbose;
    const char *conf_file;
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = static_cast<struct arguments *>(state->input);

    switch (key)
    {
    case 'v':
        arguments->verbose = 1;
        break;
    case 'c':
        arguments->conf_file = arg;
    break;

    case ARGP_KEY_ARG:
        argp_usage (state);
        break;

    case ARGP_KEY_END:
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc, nullptr, nullptr, nullptr };

struct application {
    std::unique_ptr<KnxObjectPool> pool;
    KnxScpiServerPtr scpi;
};

static struct application apps = {nullptr, nullptr};


void sig_handler(int sig)
{
    if(sig == SIGTERM || sig == SIGINT)
    {
        if(apps.pool)
        {
            log_time();
            cout << "Ask Shutdown" << endl;
            apps.pool->stop();
            apps.scpi->stop();
        }
    }
}

int main(int argc, char** argv)
{
    struct arguments arguments;
    struct sigaction sa;

    /* Default values. */
    arguments.verbose = 0;
    arguments.conf_file = "/etc/knxcached.xml";

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sig_handler;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    argp_parse (&argp, argc, argv, 0, nullptr, &arguments);

    if(access(arguments.conf_file, R_OK) != 0)
    {
        cerr << "Can't find config file (" << arguments.conf_file << ")" << endl;
        exit(1);
    }

    cout << "Using configuration file " << arguments.conf_file << endl;
    apps.pool = std::unique_ptr<KnxObjectPool>(new KnxObjectPool(arguments.conf_file));

    KnxEventFifo catchallFifo;
    thread catchallProcess(
                [&catchallFifo](){
        while(1)
        {
            KnxDataChanged data = catchallFifo.pop();
            if(data.id.size() == 0)
            {
                break;
            }
            log_time();
            cout << data.id << ": " << data.text << endl;
        }
    }
    );

    if(arguments.verbose == 1)
        apps.pool->addEventForAll(&catchallFifo);

    apps.pool->start();

    apps.scpi = KnxScpiServerPtr(new KnxScpiServer(*(apps.pool)));
    apps.scpi->start();

    /* Wait application end */

    apps.pool->join();
    apps.scpi->join();
    KnxDataChanged stop;
    catchallFifo.push(stop);
    catchallProcess.join();
    log_time();
    cout << "All thread halted" << endl;

    return 0;
}
