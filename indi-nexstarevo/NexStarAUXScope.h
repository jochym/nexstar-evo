#ifndef NEXSTARAUX_H
#define NEXSTARAUX_H

class NexStarAUXScope {

public:
    NexStarAUXScope();
    bool Abort();
    long GetALT();
    long GetAZ();
    bool GoTo(long alt, long az, bool track);
    bool Track(long altRate, long azRate);
    bool TimerTick(double dt);
    
private:
    long Alt;
    long Az;
    long AltRate;
    long AzRate;
    long targetAlt;
    long targetAz;
    long slewRate;
    bool tracking;
};


#endif // NEXSTARAUX_H
