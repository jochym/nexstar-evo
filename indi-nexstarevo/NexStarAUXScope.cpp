#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <algorithm>

#include "indicom.h"

#include "NexStarAUXScope.h"

NexStarAUXScope::NexStarAUXScope() {
    // Max slew rate (steps per second)
    slewRate=10*pow(2,24)/360;
    tracking=false;
    // Park position at the north horizon.
    Alt=targetAlt=Az=targetAz=0;
}

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

bool NexStarAUXScope::TimerTick(double dt){
    bool slewing=false;
    long da;
    int dir;
    
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
        targetAz=(Az+=AzRate);
    }
    return true;
};

