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
#include <queue>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ev++.h>

#include "indicom.h"
#include "NexStarAUXScope.h"

#define BUFFER_SIZE 10240
#define DEFAULT_ADDRESS "1.2.3.4"
#define DEFAULT_PORT 2000
int MAX_CMD_LEN=32;
/*
ev::io iow;
ev::timer tickw;
*/



void dumpMsg(buffer buf){
    fprintf(stderr, "MSG: ");
    for (int i=0; i<buf.size(); i++){
        fprintf(stderr, "%02x ", buf[i]);
    }
    fprintf(stderr, "\n");
}



void AUXCommand::fillBuf(buffer &buf){
    buf.resize(len+3);
    buf[0]=0x3b;
    buf[1]=(unsigned char)len;
    buf[2]=(unsigned char)src;
    buf[3]=(unsigned char)dst;
    buf[4]=(unsigned char)cmd;
    for (int i=5; i<len; i++){
        buf[i]=buf[i-5];
    }
    buf.back()=checksum(buf);
    dumpMsg(buf);
}

void AUXCommand::parseBuf(buffer buf){
    len=buf[1];
    src=(AUXtargets)buf[2];
    dst=(AUXtargets)buf[3];
    cmd=(AUXCommands)buf[4];
    data=buffer(buf.begin()+5,buf.end()-1);
    valid=(checksum(buf)==buf.back());
    if (not valid) {
        fprintf(stderr,"Checksum error: %02x vs. %02x", checksum(buf), buf.back());
        dumpMsg(buf);
    };
}


unsigned char AUXCommand::checksum(buffer buf){
    int l=buf[1];
    int cs=0;
    for (int i=1; i<l+2; i++){
        cs+=buf[i];
    }
    //fprintf(stderr,"CS: %x  %x  %x\n", cs, ~cs, ~cs+1);
    return (unsigned char)(((~cs)+1) & 0xFF);
    //return ((~sum([ord(c) for c in msg]) + 1) ) & 0xFF
}

long AUXCommand::getPosition(){
    if (data.size()==3) {
        char buf[4];
        bzero(buf,4);
        for (int i=0;i<data.size();i++){
            buf[i+1]=data[i];
        }
        fprintf(stderr,"Angle: %f\n",ntohl(*(uint32_t *)buf)/pow(2,24));
        return ntohl(*(uint32_t *)buf);
    } else {
        return 0;
    }
}

void AUXCommand::setPosition(long p){
    uint32_t n=htonl(p);
    unsigned char *b=(unsigned char *)&n;
    fprintf(stderr,"%x -> %x %x %x %x\n", p, b[0], b[1], b[2], b[3]);
    data.resize(3);
    for (int i=0; i<3; i++) data[i]=b[i+1];
    //data.assign(b+1,b+3);
    len=6;
}


/////////////////////////////////////////////////////
// NexStarAUXScope 
/////////////////////////////////////////////////////

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
    
    sock=0;

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
    AUXCommand *altcmd= new AUXCommand(MC_GOTO_FAST,APP,ALT);
    AUXCommand *azmcmd= new AUXCommand(MC_GOTO_FAST,APP,AZM);
    altcmd->setPosition(alt);
    azmcmd->setPosition(az);
    fprintf(stderr,"ALT: ");
    for (int i=0; i<3; i++) fprintf(stderr,"%02x ", altcmd->data[i]);
    fprintf(stderr," AZM: ");
    for (int i=0; i<3; i++) fprintf(stderr,"%02x ", azmcmd->data[i]);
    fprintf(stderr,"\n");

    oq.push(*altcmd);
    oq.push(*azmcmd);
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
    unsigned char buf[BUFFER_SIZE];

    if (sock<3) return;
    
    while((n = recv(sock, buf, sizeof(buf),0)) > 0) {
        //fprintf(stderr,"Got %d bytes.", n);
        for (int i=0; i<n; ){
            if (buf[i]==0x3b) {
                int shft;
                shft=std::min(n,i+buf[i+1]+3);
                if (shft<=n) {
                    buffer b(buf+i, buf+shft);
                    //dumpMsg(b);
                    iq.push(AUXCommand(b));
                    i+=shft;
                } else {
                    fprintf(stderr,"Partial message recv. Dropped!\n");
                    i+=shft;
                }
            } else {
                i++;
            }
        }
    }
    //fprintf(stderr,"Nothing more to read\n");
    while (not iq.empty()) {
        AUXCommand m=iq.front();
        //fprintf(stderr,"(%02x) %02x -> %02x\n", m.cmd, m.src, m.dst);
        switch (m.cmd) {
            case MC_GET_POSITION:
                fprintf(stderr,"MC_GET_POSITION %02x -> %02x\n",m.src,m.dst);
                switch (m.src) {
                    case ALT:
                        Alt=m.getPosition();
                        break;
                    case AZM:
                        Az=m.getPosition();
                        break;
                    default: break;
                }
                break;
            default :
                break;
        }
        iq.pop();
    }
}

void NexStarAUXScope::writeMsgs(){
    buffer buf;
    while (not oq.empty()) {
        AUXCommand m=oq.front();
        fprintf(stderr,"Sending: (%02x) %02x -> %02x\n", m.cmd, m.src, m.dst);
        for (int i=0; i<m.data.size(); i++) fprintf(stderr,"%02x ", m.data[i]);
        fprintf(stderr,"\n");
        m.fillBuf(buf);
        send(sock, buf.data(), buf.size(), 0);
        oq.pop();
    }
    return;
    /*
    if (sock<3) return;
    fprintf(stderr,"Sending get position\n");
    buffer buf;
    AUXCommand getALT(MC_GET_POSITION,APP,ALT);
    AUXCommand getAZM(MC_GET_POSITION,APP,AZM);
    getALT.fillBuf(buf);
    //fprintf(stderr,"buffer size: %d\n",buf.size());
    //dumpMsg(buf);
    send(sock, buf.data(), buf.size(), 0);
    getAZM.fillBuf(buf);
    //dumpMsg(buf);
    send(sock, buf.data(), buf.size(), 0);
    */
}

bool NexStarAUXScope::TimerTick(double dt){
    bool slewing=false;
    long da;
    int dir;
    
    readMsgs();
    writeMsgs();
    
    if (simulator) {
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
    }
    return true;
};

