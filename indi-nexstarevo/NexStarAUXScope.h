#ifndef NEXSTARAUX_H
#define NEXSTARAUX_H

#include <netinet/in.h>
#include <ev++.h>

class NexStarAUXScope {

public:
    NexStarAUXScope(char const *ip, int port);
    NexStarAUXScope(int port);
    NexStarAUXScope(char const *ip);
    NexStarAUXScope(struct sockaddr_in addr);
    NexStarAUXScope();
    bool Abort();
    long GetALT();
    long GetAZ();
    bool GoTo(long alt, long az, bool track);
    bool Track(long altRate, long azRate);
    bool TimerTick(double dt);
    
private:
    bool initConnection(char const *ip, int port);
    bool initConnection(struct sockaddr_in addr);
    void io_cb (ev::io &w, int revents);
    void tick_cb (ev::timer &w, int revents);
    void readMsgs();
    void writeMsgs();
    long Alt;
    long Az;
    long AltRate;
    long AzRate;
    long targetAlt;
    long targetAz;
    long slewRate;
    bool tracking;
    int sock;
};


#endif // NEXSTARAUX_H
