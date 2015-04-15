#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <algorithm>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ev++.h>

#include "indicom.h"
#include "NexStarAUXScope.h"

#define BUFFER_SIZE 1024
#define DEFAULT_ADDRESS "1.2.3.4"
#define DEFAULT_PORT 2000

/*
ev::io iow;
ev::timer tickw;
*/

NexStarAUXScope::NexStarAUXScope(char const *ip, int port){
    struct sockaddr_in addr;

    bzero(&addr, sizeof(addr));
    inet_pton(AF_INET, ip, &addr.sin_addr);
    addr.sin_port = htons(port);

    initConnection(addr);
};

NexStarAUXScope::NexStarAUXScope(char const *ip){
    initConnection(ip, DEFAULT_PORT);
};

NexStarAUXScope::NexStarAUXScope(int port){
    initConnection(DEFAULT_ADDRESS, port);
};

NexStarAUXScope::NexStarAUXScope() {
    initConnection(DEFAULT_ADDRESS, DEFAULT_PORT);
};

NexStarAUXScope::NexStarAUXScope(struct sockaddr_in addr) {
    initConnection(addr);
};

bool NexStarAUXScope::initConnection(char const *ip, int port){
    struct sockaddr_in addr;

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET ;
    addr.sin_addr.s_addr=inet_addr(ip);
    addr.sin_port = htons(port);
    return initConnection(addr);
};

bool NexStarAUXScope::initConnection(struct sockaddr_in addr) {
    int addr_len = sizeof(addr);
    char pbuf[BUFFER_SIZE]="*CLOSE*";
    
    //inet_ntop(AF_INET,&addr.sin_addr,pbuf,BUFFER_SIZE);
    //perror(pbuf);

    // Max slew rate (steps per second)
    slewRate=2*pow(2,24)/360;
    tracking=false;
    // Park position at the north horizon.
    Alt=targetAlt=Az=targetAz=0;
    

    // Create client socket
    if( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
    {
      perror("socket error");
      return false;
    }

    // Connect to server socket
    if(connect(sock, (struct sockaddr *)&addr, sizeof addr) < 0)
    {
      perror("Connect error");
      return false;
    }
    int flags;
    flags = fcntl(sock,F_GETFL,0);
    assert(flags != -1);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    fprintf(stderr, "Socket:%d\n", sock);
    /*
    iow.set <NexStarAUXScope, &NexStarAUXScope::io_cb> (this);
    iow.start(sock, ev::READ);
    tickw.set <NexStarAUXScope, &NexStarAUXScope::tick_cb> (this);
    tickw.start(5,0);
    */
//    ev::default_loop loop;
//    loop.run(0);
    return true;
};

void NexStarAUXScope::io_cb (ev::io &w, int revents) { 
    int n;
    char buf[BUFFER_SIZE];

    perror("RCV");
    while((n = recv(w.fd, buf, sizeof(buf),0)) > 0) {
          buf[n] = 0;
          for (int i=0 ; i<n ; i++) {
            printf("%02x ", buf[i]);
          }
          printf("\n");
    }
};

void NexStarAUXScope::tick_cb (ev::timer &w, int revents) { 
    perror("TICK");
};


bool NexStarAUXScope::Abort(){
    return true;
};

long NexStarAUXScope::GetALT(){
    return Alt;
};

long NexStarAUXScope::GetAZ(){
    return Az;
};

bool NexStarAUXScope::GoTo(long alt, long az, bool track){
    targetAlt=alt;
    targetAz=az;
    tracking=track;
    return true;
};

bool NexStarAUXScope::Track(long altRate, long azRate){
    AltRate=altRate;
    AzRate=azRate;
    tracking=true;
    return true;
};

void NexStarAUXScope::readMsgs(){
    int n;
    char buf[BUFFER_SIZE];

    while((n = recv(sock, buf, sizeof(buf),0)) > 0) {
          buf[n] = 0;
          for (int i=0 ; i<n ; i++) {
            fprintf(stderr, "%02x ", buf[i]);
          }
          fprintf(stderr, "\n");
    }

}

void NexStarAUXScope::writeMsgs(){

}

bool NexStarAUXScope::TimerTick(double dt){
    bool slewing=false;
    long da;
    int dir;
    
    readMsgs();
    writeMsgs();
    
    // update both axis
    if (Alt!=targetAlt){
        da=targetAlt-Alt;
        dir=(da>0)?1:-1;
        Alt+=dir*std::max(std::min(long(abs(da)/2),slewRate),1L);
        slewing=true;
    } 
    if (Az!=targetAz){
        da=targetAz-Az;
        dir=(da>0)?1:-1;
        Az+=dir*std::max(std::min(long(abs(da)/2),slewRate),1L);
        slewing=true;
    }
    
    // if we reach the target at previous tick start tracking if tracking requested
    if (tracking && !slewing && Alt==targetAlt && Az==targetAz) {
        targetAlt=(Alt+=AltRate*dt);
        targetAz=(Az+=AzRate*dt);
    }
    return true;
};

