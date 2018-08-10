#ifndef COMMON_H
#define COMMON_H

#include <ctime>
#include <cstdio>

inline void log_time()
{
    time_t t = time(nullptr);   // get time now
    struct tm * now = localtime( & t );
    printf("%i/%02i/%02i %02i:%02i:%02i: ",
           now->tm_year + 1900,
           now->tm_mon + 1,
           now->tm_mday,
           now->tm_hour,
           now->tm_min,
           now->tm_sec);

}

#endif // COMMON_H
