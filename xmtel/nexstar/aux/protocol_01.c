/* -------------------------------------------------------------------------- */
/* -      Protocol for NexStar auxiliary command set telescope control      - */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright 2010 John Kielkopf                                               */
/*                                                                            */
/* Distributed under the terms of the General Public License (see LICENSE)    */
/*                                                                            */
/* John Kielkopf (kielkopf@louisville.edu)                                    */
/*                                                                            */
/* Date: August 21, 2010                                                      */
/* Version: 5.2.0                                                             */
/*                                                                            */
/* History:                                                                   */
/*                                                                            */
/* January 1, 2009                                                            */
/*   Version 1.0                                                              */
/*     Released                                                               */
/*                                                                            */
/* July 27, 2009                                                              */
/*   Version 1.1                                                              */
/*     Home position included                                                 */
/*                                                                            */
/* August 12, 2009                                                            */
/*   Version 1.2                                                              */
/*     Encoder count to alt and az mapping error fixed                        */
/*                                                                            */
/* October 28, 2009                                                           */
/*   Version 1.3                                                              */
/*     GEM alt encoder zero point changed                                     */
/*     Encoder count to ra/dec rewritten                                      */
/*                                                                            */
/* November 12, 2009                                                          */
/*   Version 1.3                                                              */
/*     Focus encoder count corrected                                          */
/*                                                                            */
/* November 14, 2009                                                          */
/*   Version 1.4                                                              */
/*     GEM alt encoder sign error fixed                                       */
/*     Focus encoder calibrated in microns                                    */
/*                                                                            */
/* November 14, 2009                                                          */
/*   Version 1.5                                                              */
/*     Fan control and temperature readout added                              */
/*                                                                            */
/* November 28, 2009                                                          */
/*   Version 1.6                                                              */
/*     Focus calibration moved to header file                                 */
/*                                                                            */
/* November 30, 2009                                                          */
/*   Version 1.7                                                              */
/*     Modified slew across meridian                                          */
/*                                                                            */
/* December 2, 2009                                                           */
/*   Version 1.8                                                              */
/*     Corrected reading on following east to west across meridian            */
/*                                                                            */
/* December 21, 2009                                                          */
/*   Version 1.9                                                              */
/*     Two phase slew modified near the pole                                  */
/*                                                                            */
/* August 11, 2010                                                            */
/*   Version 5.2.0                                                            */
/*     Bumped version number to match parent xmtel                            */
/*     Removed unused timer count from focus encoder routine                  */
/*     Changed SyncTelOffsets to return void                                  */
/*     Removed time sync routines that were not used                          */
/*     Corrected apparent error in GoToCoords where we had not set            */
/*       pmodel=RAW before asking for initial mount coordinates               */
/*     Added device option for serial port                                    */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <math.h>
#include "protocol.h"

#define NULL_PTR(x) (x *)0

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* There are two classes of routines defined here:                            */

/*   XmTel commands to allow generic access to the controller.                */
/*   NexStar specific commands and data.                                      */


/* System variables and prototypes */

/* Telescope and mounting commands that may be called externally */

/* Interface control */

void ConnectTel(void);
int  SetTelEncoders(double homeha, double homedec);
void DisconnectTel(void);
int  CheckConnectTel(void);

/* Slew and track control */

void SetRate(int newRate);
void StartSlew(int direction);
void StopSlew(int direction);
void StartTrack(void);
void StopTrack(void);
void FullStop(void);

/* Coordinates */

void GetTel(double *telra, double *teldec, int pmodel);
int  GoToCoords(double newRA, double newDec, int pmodel);
int  CheckGoTo(double desRA, double desDec, int pmodel);

/* Slew limits */

int  GetSlewStatus(void);
int  SetLimits(int limits);
int  GetLimits(int *limits);

/* Synchronizing */

int  SyncTelOffsets(double newoffsetra, double newoffsetdec);
int  SyncTelToUTC(double newutc);
int  SyncTelToLocation(double newlong, double newlat, double newalt);
int  SyncTelToLST(double newTime);

/* Instrumentation */

void Heater(int heatercmd);
void Fan(int fancmd);
void Focus(int focuscmd, int focusspd);
void Rotate(int rotatecmd, int rotatespd);
void GetFocus(double *telfocus);
void GetRotate(double *telrotate);
void GetTemperature(double *teltemperature);

/* External variables and shared code */

extern double LSTNow(void);
extern double UTNow(void);
extern double Map24(double hour);
extern double Map12(double hour);
extern double Map360(double degree);
extern double Map180(double degree);
extern double offsetra, offsetdec;
extern double SiteLatitude, SiteLongitude;
extern int    telmount;
extern int    homenow;
extern double homeha;            /* Home ha */
extern double homera;            /* Home ra derived from ha */
extern double homedec;           /* Home dec */ 
extern char   telserial[32];     /* Serial port */

extern void PointingFromTel (double *telra1, double *teldec1, 
  double telra0, double teldec0, int pmodel);

extern void PointingToTel (double *telra0, double *teldec0, 
  double telra1, double teldec1, int pmodel);
  
extern void EquatorialToHorizontal(double ha, double dec, double *az, double *alt);
extern void HorizontalToEquatorial(double az, double alt, double *ha, double *dec);  


/* NexStar local data */

static int slewRate;                  /* Rate for slew request in StartSlew */
static int slewphase = 0;             /* Slew sequence counter */
static double telencoderalt = 0.;     /* Global encoder angle updated by GetTel */
static double telencoderaz = 0.;      /* Global encoder angle updated by GetTel */
static double switchalt = 0.;         /* Global encoder angle of switch position */
static double switchaz = 0.;          /* Global encoder angle of switch position */

/* NexStar calibration data */

static double altcountperdeg = ALTCOUNTPERDEG;
static double azcountperdeg  = AZCOUNTPERDEG;

/* NexStar focus data */

static double focusscale = FOCUSSCALE;
static int focusdir = FOCUSDIR;

 

/* Communications variables and routines for internal use */

static int TelPortFD;
static int TelConnectFlag = FALSE;

typedef fd_set telfds;

static int readn(int fd, char *ptr, int nbytes, int sec);
static int writen(int fd, char *ptr, int nbytes);
static int telstat(int fd,int sec,int usec);

/* End of prototype and variable definitions */

/* Handcontroller auxiliary command interfacing notes                        */
/*                                                                           */
/* The command format for a connection through the hand controller is        */  
/*                                                                           */
/*   0x50          code requesting that the data pass through to the motors  */    
/*   msgLen        how many bytes including msgId  and valid data bytes      */
/*   destId        AUX command set destination number                        */
/*   msgId         AUX command set message number                            */
/*   data1-3       bytes of message data                                     */
/*   responseBytes number of bytes in response                               */
/*                                                                           */
/* For example, the command to get data from the focus motor is              */
/*                                                                           */
/*   focusCmd[] = { 0x50, 0x01, 0x12, 0x01, 0x00, 0x00, 0x00, 0x03 }         */
 


/* Report on telescope connection status */

int CheckConnectTel(void)
{
  if (TelConnectFlag == TRUE)
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}


/* Connect to the telescope serial interface */
/* Returns without action if TelConnectFlag is TRUE */
/* Sets TelConnectFlag TRUE on success */

void ConnectTel(void)
{  
  struct termios tty;
  
  /* Packet to request version of azimuth motor driver */
  
  char sendstr[] = { 0x50, 0x01, 0x10, 0xfe, 0x00, 0x00, 0x00, 0x02 };
      
  /* Packet format:              */
  /*   preamble                  */
  /*   packet length             */
  /*   destination               */
  /*   message id                */
  /*   three message bytes       */
  /*   number of response bytes  */
   
  char returnstr[32];
  
  /* Packet format:              */
  /*   response bytes if any     */
  /*   #                         */
  
  int numRead;
  int limits, flag;
  
  if(TelConnectFlag != FALSE)
  {
    return;
  }
  
  /* Make the connection */
  
  /* TelPortFD = open("/dev/ttyS0",O_RDWR); */
  
  TelPortFD = open(telserial,O_RDWR);
  if(TelPortFD == -1)
  {
    fprintf(stderr,"Serial port not available ... \n");
    return;
  }
  
  tcgetattr(TelPortFD,&tty);
  cfsetospeed(&tty, (speed_t) B9600);
  cfsetispeed(&tty, (speed_t) B9600);
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
  tty.c_iflag =  IGNBRK;
  tty.c_lflag = 0;
  tty.c_oflag = 0;
  tty.c_cflag |= CLOCAL | CREAD;
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 5;
  tty.c_iflag &= ~(IXON|IXOFF|IXANY);
  tty.c_cflag &= ~(PARENB | PARODD);
  tcsetattr(TelPortFD, TCSANOW, &tty);

  /* Flush the input (read) buffer */

  tcflush(TelPortFD,TCIOFLUSH);

  /* Test connection by asking for version of azimuth motor */

  writen(TelPortFD,sendstr,8);
  numRead=readn(TelPortFD,returnstr,3,2);
  
  if (numRead == 3) 
  {
    fprintf(stderr,"RA/Azimuth ");
    fprintf(stderr,"controller version %d.%d ", returnstr[0], returnstr[1]);
    fprintf(stderr,"connected \n");    
  }
  else
  {
    fprintf(stderr,"RA/Azimuth drive not responding ...\n");
    return;
  }  

  /* Test connection by asking for version of altitude motor */
  
  sendstr[2] = 0x11;

  /* Flush the input buffer */

  tcflush(TelPortFD,TCIOFLUSH);
  
  /* Send the request */
  
  writen(TelPortFD,sendstr,8);
  numRead=readn(TelPortFD,returnstr,3,2);
  
  /* Add null terminator to simplify handling the return data */
  
  returnstr[numRead] = '\0';
   
  if (numRead == 3) 
  { 
    fprintf(stderr,"Declination/Altitude ");
    fprintf(stderr,"controller version %d.%d ", returnstr[0], returnstr[1]);
    fprintf(stderr,"connected\n");    
    TelConnectFlag = TRUE;
  }
  else
  {
    fprintf(stderr,"Declination/altitude drive not responding ...\n");
    return;
  }  
   
  /* Perform startup tests */

  flag = GetLimits(&limits);
  usleep(500000);
  limits = FALSE;
  flag = SetLimits(limits);
  usleep(500000);  
  flag = GetLimits(&limits);
  
  /* Set global switch angles for a GEM OTA over the pier pointing at pole   */
  /* They correspond to ha ~ -6 hr and dec ~ +90 deg for northern telescope  */
  /* They correspond to ha ~ +6 hr and dec ~ -90 deg for southern telescope  */
  /* Non-zero values work better in goto routines on nexstar                 */
      
  switchaz = 1.; 
  switchalt = -1.0;
  
  /* Hardcoded defaults for homeha and homedec are overridden by prefs */
  /* GEM: over the pier pointing at the pole */
  /* EQFORK: pointing at the equator on the meridian */
  /* ALTAZ: level and pointing north */

  if ( homenow != TRUE )
  {
    if (telmount == GEM)
    {
      if (SiteLatitude < 0.)
      {
        homedec = -89.9999;
        homeha = 6.;
      }
      else
      {  
        homedec = 89.9999;
        homeha = -6.;
      }
    }
    else if (telmount == EQFORK)
    {
      if (SiteLatitude < 0.)
      {
        homedec = 0.;
        homeha = 0.;
      }
      else
      {  
        homedec = 0.;
        homeha = 0.;
      }
    }
    else if (telmount == ALTAZ)
    {
      /* Set azimuth */
      homeha = 0.;    
      
      /* Set altitude */
      homedec = 0.; 
    }
    else
    {
      fprintf(stderr,"Telescope mounting must be GEM, EQFORK, or ALTAZ\n");
      return;
    }
  } 
   

  fprintf(stderr, "Using initial HA: %lf\n", homeha);
  fprintf(stderr, "Using initial Dec: %lf\n",homedec); 
    
  flag = SetTelEncoders(homeha, homedec);
  if (flag != TRUE)
  {
    fprintf(stderr,"Initial telescope pointing request was out of range ... \n");
    return;
  }
   
  /* Read encoders and confirm pointing */
  
  GetTel(&homera, &homedec, RAW);
  
  fprintf(stderr, "Local latitude: %lf\n", SiteLatitude);
  fprintf(stderr, "Local longitude: %lf\n", SiteLongitude);
  fprintf(stderr, "Local sidereal time: %lf\n", LSTNow()); 
  fprintf(stderr, "Mount type: %d\n", telmount);
  fprintf(stderr, "Mount now reading RA: %lf\n", homera);
  fprintf(stderr, "Mount now reading Dec: %lf\n", homedec);
  
  fprintf(stderr, "The telescope is running ...\n\n");
    
  /* Flush the input buffer in case there is something left from startup */

  tcflush(TelPortFD,TCIOFLUSH);

}

/* Assign and save slewRate for use in StartSlew */

void SetRate(int newRate)
{
  if(newRate == SLEW) 
    {
      slewRate = 9; 
    }
  else if(newRate == FIND) 
    {
      slewRate = 6;
    }
  else if(newRate == CENTER) 
    {
      slewRate = 3;
    }
  else if(newRate == GUIDE) 
    {
      slewRate = 1;
    }
}
 

/* Start a slew in chosen direction at slewRate */
/* Use auxilliary NexStar command set through the hand control computer */

void StartSlew(int direction)
{
  char slewCmd[] = { 0x50, 0x02, 0x11, 0x24, 0x09, 0x00, 0x00, 0x00 };
  char inputstr[2048];
  
  if(direction == NORTH)
    {
      slewCmd[2] = 0x11; 
      slewCmd[3] = 0x24;
      slewCmd[4] = slewRate;
    }
  else if(direction == EAST)
    {
      slewCmd[2] = 0x10; 
      slewCmd[3] = 0x25;
      slewCmd[4] = slewRate;
    }
  else if(direction == SOUTH)
    {
      slewCmd[2] = 0x11; 
      slewCmd[3] = 0x25;
      slewCmd[4] = slewRate;
    }
  else if(direction == WEST)
    {
      slewCmd[2] = 0x10; 
      slewCmd[3] = 0x24;
      slewCmd[4] = slewRate;
    }

  writen(TelPortFD,slewCmd,8);

  /* Look for '#' acknowledgement of request*/

  for (;;) 
  {
    if ( readn(TelPortFD,inputstr,1,1) ) 
    {
      if (inputstr[0] == '#') break;
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from telescope slew control\n");
    }
  }   
}


/* Stop the slew in chosen direction */

void StopSlew(int direction)
{
  char slewCmd[] = { 0x50, 0x02, 0x11, 0x24, 0x00, 0x00, 0x00, 0x00 };
  char inputstr[2048];
  
  if(direction == NORTH)
    {
      slewCmd[2] = 0x11; 
      slewCmd[3] = 0x24;
    }
  else if(direction == EAST)
    {
      slewCmd[2] = 0x10; 
      slewCmd[3] = 0x24;
    }
  else if(direction == SOUTH)
    {
      slewCmd[2] = 0x11; 
      slewCmd[3] = 0x24;
    }
  else if(direction == WEST)
    {
      slewCmd[2] = 0x10; 
      slewCmd[3] = 0x24;
    }

  tcflush(TelPortFD,TCIOFLUSH);

  writen(TelPortFD,slewCmd,8);

  /* Look for '#' acknowledgement of request*/

  for (;;) 
  {
    if ( readn(TelPortFD,inputstr,1,1) ) 
    {
      if (inputstr[0] == '#') break;
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from telescope slew control\n");
    }
  }  
}

void DisconnectTel(void)
{
  /* printf("DisconnectTel\n"); */
  if(TelConnectFlag == TRUE)
    close(TelPortFD);
  TelConnectFlag = FALSE;
}


/* Set the encoder count for current ha and dec                           */
/* Requires access to global telmount                                     */
/* Returns +1 (TRUE) if operation is allowed and 0 (FALSE) otherwise      */

int SetTelEncoders(double setha, double setdec)
{
  /* Packet to set ha/azimuth drive  */

  char sendstr[] = { 0x50, 0x04, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00 };
  
  /* Packet for return string */
  
  char returnstr[32];
    
  /* Count registers */
  
  int azcount, azcount0, azcount1, azcount2;
  int altcount, altcount0, altcount1, altcount2;
  
  /* Used to count number of characters read in the return packet */
  
  int numread; 
  
  /* Temporary variables for altitude and azimuth in degrees */
  
  double altnow, aznow; 
  
  /* Temporary variables for real encoder readings in degrees */
  
  double encoderalt = 0.;
  double encoderaz = 0.;
  
  /* Test ha and dec for valid range */
  
  if ( setha > 6. || setha < -6. )
  {
    fprintf(stderr,"Telescope may be pointed outside valid HA range\n");
    fprintf(stderr,"It cannot be started safely using last known position\n");
    return(0);
  }
  
  if ( setdec > 90. || setdec < -90. )
  {
    fprintf(stderr,"Telescope may be pointed outside valid Dec range\n");
    fprintf(stderr,"It cannot be started safely using last known position\n");
    return(0);
  }
  
  /* Convert HA and Dec to Alt and Az */
  
  EquatorialToHorizontal(setha, setdec, &aznow, &altnow);
  
  if (altnow < 0.)
  {
    fprintf(stderr,"Telescope may be pointed below the horizon\n");
    fprintf(stderr,"It cannot be started safely using last known position\n");
    return(0);
  }
                
  /* Convert coordinates to counts                             */
  /* Zero points:                                              */
  /* Alt-Az -- horizon north                                   */
  /* Fork equatorial -- equator meridian                       */
  /* German equatorial -- over the mount at the pole           */
  /* Sign of encolder scales assumed:                          */
  /* Dec or Alt -- increase from equator to mount pole         */ 
  /* HA or Az   -- increase from east to west                  */
  /* Counts wrap signed 24-bit integer where 16777216 is 2^24  */
    
  if (telmount == GEM)
  {

    /* Flip signs if sited below the equator */
    
    if (SiteLatitude < 0.)
    {
      setdec = -1.*setdec;
      setha = -1.*setha;
    }
    
    /* Encoder az increases as this resigned ha increases */
    /* Zero point is with telescope over the pier pointed at pole */     
        
    if (setha == 0.)
    { 
      /* OTA looking at meridian */
      /* This is ambiguous unless we know which side of the pier it is on */
      /* Assume telescope was on the west side looking east */
      /*   and was moved to point to the meridian with the OTA west of pier */
      
      fprintf(stderr,"Warning: assuming OTA is west of pier.\n");
      encoderaz = 90.;
      encoderalt = setdec - 90.;
    }
    else if (setha == -6.)
    {
      /* OTA looking east */
      encoderaz = 0.;
      encoderalt = setdec - 90.;
    }
    else if (setha == 6.)
    {
      /* OTA looking west */
      encoderaz = 0.;
      encoderalt = 90. - setdec;
    }    
    else if ((setha > -12.) && ( setha < -6.))
    { 
      /* OTA east of pier looking below the pole */
      encoderaz = setha*15. + 90.;
      encoderalt = setdec - 90.;
    }
    else if ((setha > -6.) && ( setha < 0.))
    { 
      /* OTA west of pier looking east */
      encoderaz = setha*15. + 90.;
      encoderalt = setdec - 90.;
    }
    else if ((setha > 0.) && ( setha <= 6.))
    { 
      /*OTA east of pier looking west */
      encoderaz = setha*15. - 90.;
      encoderalt = 90. - setdec;
    }
    else if ((setha > 6.) && ( setha < 12.))
    { 
      /* OTA west of pier looking below the pole */
      encoderaz = setha*15. - 90.;
      encoderalt = 90. - setdec;
    }    
    else
    {
      fprintf(stderr,"German equatorial outside limits\n");
      return(0);      
    }

    encoderaz = encoderaz*azcountperdeg;
    encoderalt = encoderalt*altcountperdeg;            
  }
  else if (telmount == EQFORK)
  {
    
    /* Flip signs for the southern sky */
    
    if (SiteLatitude < 0.)
    {
      setdec = -1.*setdec;
      setha = -1.*setha;
    }

    if ((setha > -6.) && ( setha < 6.))
    { 
      encoderaz = setha*15.;
      encoderalt = setdec;
      encoderaz = encoderaz*azcountperdeg;
      encoderalt = encoderalt*altcountperdeg;       
    }
    else
    {
      fprintf(stderr,"Equatorial fork outside valid HA range\n");
      return(0);      
    }
  }  
  else if (telmount == ALTAZ)
  {
    encoderaz = aznow*azcountperdeg;
    encoderalt = altnow*altcountperdeg;
  }
  else
  {
    fprintf(stderr,"Unknown mount type\n");
    return(0);
  }   
    
  azcount = encoderaz;
  if (azcount < 0)
  {
    azcount = 16777217 + azcount;
  }

  altcount = encoderalt;
  if (altcount < 0)
  {
    altcount = 16777217 + altcount;
  }

  /* Parse each of the 3 bytes of the counters */
    
  azcount0  = azcount  / 65536;
  azcount   = azcount  % 65536;
  azcount1  = azcount  / 256;
  azcount2  = azcount  % 256;
  altcount0 = altcount / 65536;
  altcount  = altcount % 65536;
  altcount1 = altcount / 256;
  altcount2 = altcount % 256;
  
  /* Set RA/Azimuth encoder to this position */
    
  sendstr[1] = 0x04;
  sendstr[2] = 0x10;
  sendstr[3] = 0x04;
  sendstr[4] = (unsigned short) azcount0;
  sendstr[5] = (unsigned short) azcount1;
  sendstr[6] = (unsigned short) azcount2;

  tcflush(TelPortFD,TCIOFLUSH);
  
  writen(TelPortFD,sendstr,8);
  numread=readn(TelPortFD,returnstr,1,2);
  
  /* Set Dec/Altitude encoder to this position */
    
  sendstr[1] = 0x04;
  sendstr[2] = 0x11;
  sendstr[3] = 0x04;
  sendstr[4] = (unsigned short) altcount0;
  sendstr[5] = (unsigned short) altcount1;
  sendstr[6] = (unsigned short) altcount2;

  tcflush(TelPortFD,TCIOFLUSH);
    
  writen(TelPortFD,sendstr,8);
  numread=readn(TelPortFD,returnstr,1,2);
  
  tcflush(TelPortFD,TCIOFLUSH);

  return(1);
}

/* Read the mount encoders                                            */
/* Requires access to global telmount                                 */
/* Save encoder angle readings in global telencoder variables         */
/* Convert the encoder readings to mounting ha and dec                */
/* Use an NTP synchronized clock to find lst and ra                   */
/* Correct for the pointing model to find the true direction vector   */
/* Report the ra and dec at which the telescope is pointed            */

void GetTel(double *telra, double *teldec, int pmodel)
{  
   
  /* Packet preset to request HA/Azimuth counts */

  char sendstr[] = { 0x50, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x03 };
  char returnstr[32];
  int azcount = 0; 
  int altcount = 0;
  double encoderaz = 0.;
  double encoderalt = 0.;
  double telha0 = 0.;
  double teldec0 = 0.;
  double telra0 = 0.;
  double telra1 = 0.;
  double teldec1 = 0.;
  int numRead = 0;
  static int count0, count1, count2;

  /* Packet to request RA/Azimuth */
  
  sendstr[2] = 0x10;
  
  /* Flush the input buffer */
  
  tcflush(TelPortFD,TCIOFLUSH);
  
  /* Send the request */

  writen(TelPortFD,sendstr,8);
  numRead=readn(TelPortFD,returnstr,4,2);

  if (numRead == 4) 
  {         
    count0 = (unsigned char) returnstr[0];
    count1 = (unsigned char) returnstr[1];
    count2 = (unsigned char) returnstr[2];
    azcount = 256*256*count0 + 256*count1 + count2;
    if (azcount > 8388608)
    {
      azcount = -(16777217 - azcount);
    }
    encoderaz = (double) azcount;
  }

  /* Packet to request Dec/Altitude */
  
  sendstr[2] = 0x11;

  /* Flush the input buffer */
  
  tcflush(TelPortFD,TCIOFLUSH);

  /* Send the request */
  
  writen(TelPortFD,sendstr,8);
  numRead=readn(TelPortFD,returnstr,4,2);
  
  if (numRead == 4) 
  {     
    count0 = (unsigned char) returnstr[0];
    count1 = (unsigned char) returnstr[1];
    count2 = (unsigned char) returnstr[2];
    altcount = 256*256*count0 + 256*count1 + count2;
    
    if (altcount > 8388608)
    {
      altcount = -(16777217 - altcount);
    }   
    encoderalt = (double) altcount;
  }  
  
  /* Convert counts to degrees for both azimuth and altitude encoders */
  
  encoderaz = encoderaz / azcountperdeg;
  encoderalt = encoderalt / altcountperdeg;
  
  /* Transform encoder readings to mount ha, ra and dec */
  /* GEM encoders zero for OTA over pier pointed at pole */
  
  if (telmount == GEM)
  {
    if ( encoderaz == 0. ) 
    {
      if ( encoderalt < 0.)
      {
        telha0 = -6.;
        teldec0 = 90. + encoderalt;
      }
      else
      {
        telha0 = +6.;
        teldec0 = 90. - encoderalt;
      }
    }
    else if ( encoderaz == -90. )
    {
      if ( encoderalt < 0.)
      {
        telha0 = 0.;
        teldec0 = 90. + encoderalt;
      }
      else
      {
        telha0 = -12.;
        teldec0 = 90. - encoderalt;
      }    
    }
    else if ( encoderaz == 90. )
    {
      if ( encoderalt > 0.)
      {
        telha0 = 0.;
        teldec0 = 90. - encoderalt;
      }
      else
      {
        telha0 = -12.;
        teldec0 = 90. + encoderalt;
      }    
    }   
    else if ((encoderaz > -180. ) && (encoderaz < -90.))
    {
      teldec0 = 90. - encoderalt;
      telha0 = Map12(6. + encoderaz/15.);  
    }
    else if ((encoderaz > -90. ) && (encoderaz < 0.))
    {
      teldec0 = 90. - encoderalt;
      telha0 = Map12(6. + encoderaz/15.);  
    }    
    else if ((encoderaz > 0. ) && (encoderaz < 90.))
    {
      teldec0 = 90. + encoderalt;
      telha0 = Map12(-6. + encoderaz/15.);  
    }    
    else if ((encoderaz > 90. ) && (encoderaz < 180.))
    {
      teldec0 = 90. + encoderalt;
      telha0 = Map12(-6. + encoderaz/15.);  
    }   
    else
    {
      fprintf(stderr,"German equatorial ha encoder out of range\n");      
      teldec0 = 0.;
      telha0 = 0.;
    }   
    
    /* Flip signs for the southern sky */
    
    if (SiteLatitude < 0.)
    {
      teldec0 = -1.*teldec0;
      telha0 = -1.*telha0;
    }
        
    telra0 = Map24(LSTNow() - telha0);
  }
    
  else if (telmount == EQFORK)
  {
    teldec0 = encoderalt;
    telha0 = Map12(encoderaz/15.);
    
    /* Flip signs for the southern sky */
    
    if (SiteLatitude < 0.)
    {
      teldec0 = -1.*teldec0;
      telha0 = -1.*telha0;
    }
        
    telra0 = Map24(LSTNow() - telha0);    
  }
  
  else if (telmount == ALTAZ)
  {
    HorizontalToEquatorial(encoderaz, encoderalt, &telha0, & teldec0);
    telha0 = Map12(telha0);
    telra0 = Map24(LSTNow() - telha0);   
  }

  else
  {
    fprintf(stderr,"Unknown mounting type\n");  
    *telra=0.;
    *teldec=0.;
    telencoderaz = 0.;
    telencoderalt = 0.;
    return;
  }
    
  /* Handle special case if not already treated where dec is beyond a pole */
  
  if (teldec0 > 90.)
  {
    teldec0 = 180. - teldec0;
    telra0 = Map24(telra0 + 12.);
  }
  else if (teldec0 < -90.)
  {
    teldec0 = -180. - teldec0;
    telra0 = Map24(telra0 + 12.);
  }
    
  /* Apply pointing model to the coordinates that are reported by the telescope */
  
  PointingFromTel(&telra1, &teldec1, telra0, teldec0, pmodel);
      
  /* Return corrected values */

  *telra=telra1;
  *teldec=teldec1;
  
  /* Update global encoder reading */
  
  telencoderaz =  encoderaz;
  telencoderalt = encoderalt;

  return;

}


/* Go to new celestial coordinates                                    */
/* Evaluate if target coordinates are valid                           */
/* Test slew limits in altitude, polar, and hour angles               */
/* Query if target is above the horizon                               */
/* Return without action for invalid requests                         */
/* Interrupt any slew sequence in progress                            */
/* Check current pointing                                             */
/* Find encoder readings required to point at target                  */
/* Set slewphase equal to number of slew segments needed              */
/* Begin next segment needed to reach target from current pointing    */
/* Repeated calls are required when more than one segment is needed   */
/* Return 1 if underway                                               */
/* Return 0 if done or not permitted                                  */

int GoToCoords(double newra, double newdec, int pmodel)
{
  char sendstr[] = { 0x50, 0x04, 0x10, 0x17, 0x00, 0x00, 0x00, 0x00 };
  char returnstr[32];
  int azcount, azcount0, azcount1, azcount2;
  int altcount, altcount0, altcount1, altcount2;
  int numread; 
  double newha, newalt, newaz;
  double newha0, newalt0, newaz0;
  double newra0, newdec0;
  double newra1, newdec1;
  double nowha0, nowra0, nowdec0;
  double encoderalt = 0.;
  double encoderaz = 0.;
     
  /* Select fast slew command if needed */
  /* Note:  may place large inertial load on the gear train */
  
  if( SLEWFAST )
  {
    sendstr[3] = 0x02;
  }
        
  newha = LSTNow() - newra;
  newha = Map12(newha);
  
  /* Convert HA and Dec to Alt and Az */
  /* Test for visibility              */
  
  EquatorialToHorizontal(newha, newdec, &newaz, &newalt);
  
  /* Check altitude limit */
  
  if (newalt < MINTARGETALT)
  {
    fprintf(stderr,"Target is below the telescope horizon\n");
    slewphase = 0;
    return(0);
  }
  
  /* Target request is valid */
  /* Find the best path to target */
    
  /* Mount coordinates for the target */
  
  newra1 = newra;
  newdec1 = newdec;
  PointingToTel(&newra0,&newdec0,newra1,newdec1,pmodel);  
  newha0 = LSTNow() - newra0;
  newha0 = Map12(newha0);
  EquatorialToHorizontal(newha0, newdec0, &newaz0, &newalt0);
  
  /* Stop all mount motion in preparation for a slew */
  
  FullStop();  

  /* Get current mount coordinates */
  
  GetTel(&nowra0, &nowdec0, RAW);
  nowha0 = LSTNow() - nowra0;
  nowha0 = Map12(nowha0);
          
  /* Prepare encoder counts for a new slew */

  /* German equatorial */

  if (telmount == GEM)
  {
    
    /* Flip signs for the southern sky */
    
    if (SiteLatitude < 0.)
    {
      newdec0 = -1.*newdec0;
      newha0 = -1.*newha0;
    }
    
    if ((newha0 >= -12.) && ( newha0 < -6.))
    { 
      slewphase = 1;
      encoderaz = newha0*15. + 180.;
      encoderalt = newdec0;
    }
    else if ((newha0 >= -6.) && ( newha0 <= 0.))
    { 
      slewphase = 1;
      encoderaz = newha0*15. + 180.;
      encoderalt = newdec0;
    }
    else if ((newha0 > 0.) && ( newha0 <= 6.))
    { 
      slewphase = 1;
      encoderaz = newha0*15.;
      encoderalt = 180. - newdec0;
    }
    else if ((newha0 > 6.) && ( newha0 <= 12.))
    { 
      slewphase = 1;
      encoderaz = newha0*15.;
      encoderalt = 180. - newdec0;
    }    
    else
    {
      fprintf(stderr,"German equatorial slew request error\n");
      return(0);      
    }

    if (newha0 == 0.)
    { 
      /* OTA looking at meridian */
      /* This is ambiguous unless we know which side of the pier it is on */
      /* Assume telescope was on the west side looking east */
      /*   and was moved to point to the meridian with the OTA west of pier */
      slewphase = 1;
      fprintf(stderr,"Warning: assuming OTA is west of pier.\n");
      encoderaz = 90.;
      encoderalt = newdec0 - 90.;
    }
    else if (newha0 == -6.)
    {
      /* OTA looking east */
      slewphase = 1;
      encoderaz = 0.;
      encoderalt = newdec0 - 90.;
    }
    else if (newha0 == 6.)
    {
      /* OTA looking west */
      slewphase = 1;
      encoderaz = 0.;
      encoderalt = 90. - newdec0;
    }    
    else if ((newha0 > -12.) && ( newha0 < -6.))
    { 
      /* OTA east of pier looking below the pole */
      slewphase = 1;
      encoderaz = newha0*15. + 90.;
      encoderalt = newdec0 - 90.;
    }
    else if ((newha0 > -6.) && ( newha0 < 0.))
    { 
      /* OTA west of pier looking east */
      slewphase = 1;
      encoderaz = newha0*15. + 90.;
      encoderalt = newdec0 - 90.;
    }
    else if ((newha0 > 0.) && ( newha0 <= 6.))
    { 
      /*OTA east of pier looking west */
      slewphase = 1;
      encoderaz = newha0*15. - 90.;
      encoderalt = 90. - newdec0;
    }
    else if ((newha0 > 6.) && ( newha0 < 12.))
    { 
      /* OTA west of pier looking below the pole */
      slewphase = 1;
      encoderaz = newha0*15. - 90.;
      encoderalt = 90. - newdec0;
    }    
    else
    {
      fprintf(stderr,"German equatorial slew request outside limits\n");
      return(0);      
    }

    /* Tests for safe slew based on encoder readings would go here */
        
    /* Test need for two-segment slew for changes of more than 90 degrees */
    
    if ((fabs(telencoderalt - encoderalt) > 90.1) || 
      (fabs(telencoderaz - encoderaz) > 90.1))
    {
      
      /* Slew request of more than 90 degrees on one axis */
      
      if ( fabs(telencoderalt) > 10. )
      {
        
        /* Telescope currently more than 10 degrees from the pole in dec */
        /* Set new target to switch position */
        
        encoderalt = switchalt;
        encoderaz = switchaz;
        slewphase = 2;
      }  
    }
            
    encoderalt = encoderalt*altcountperdeg;
    encoderaz = encoderaz*azcountperdeg;     
  }
  
  /* Equatorial fork */
  
  if (telmount == EQFORK)
  {
    slewphase = 1;
    encoderaz = newha0*15.;
    encoderalt = newdec0;

    /* Tests for safe slew based on encoder readings would go here */

    encoderalt = encoderalt*altcountperdeg;
    encoderaz = encoderaz*azcountperdeg;       
  }
  
  /* Alt-az fork */
  
  if (telmount == ALTAZ)
  {
    slewphase = 1;
    encoderaz = newaz0;
    encoderalt = newalt0;

    /* Tests for safe slew based on encoder readings would go here */

    encoderaz = encoderaz*azcountperdeg;
    encoderalt = encoderalt*altcountperdeg;
  }
        
  /* Convert encoder angle readings to encoder counter readings */

  azcount = encoderaz;
  if (azcount < 0)
  {
    azcount = 16777217 + azcount;
  }

  altcount = encoderalt;
  if (altcount < 0)
  {
    altcount = 16777217 + altcount;
  }

  /* Prepare NexStar commands                  */
  /* Parse each of the 3 bytes of the counters */
    
  azcount0  = azcount  / 65536;
  azcount   = azcount  % 65536;
  azcount1  = azcount  / 256;
  azcount2  = azcount  % 256;
  altcount0 = altcount / 65536;
  altcount  = altcount % 65536;
  altcount1 = altcount / 256;
  altcount2 = altcount % 256;
  
  /* Send command to go to new RA/Azimuth */
    
  sendstr[1] = 0x04;
  sendstr[2] = 0x10;
  sendstr[3] = 0x17;
  sendstr[4] = (unsigned short) azcount0;
  sendstr[5] = (unsigned short) azcount1;
  sendstr[6] = (unsigned short) azcount2;

  tcflush(TelPortFD,TCIOFLUSH);
  
  writen(TelPortFD,sendstr,8);
  numread=readn(TelPortFD,returnstr,1,2);
  
  /* Send command to go to new Dec/Altitude */
    
  sendstr[1] = 0x04;
  sendstr[2] = 0x11;
  sendstr[3] = 0x17;
  sendstr[4] = (unsigned short) altcount0;
  sendstr[5] = (unsigned short) altcount1;
  sendstr[6] = (unsigned short) altcount2;

  tcflush(TelPortFD,TCIOFLUSH);
    
  writen(TelPortFD,sendstr,8);
  numread=readn(TelPortFD,returnstr,1,2);
  
  tcflush(TelPortFD,TCIOFLUSH);
  
  /* A slew is in progress */

  return(1);
}

/* Low level check of slew status on both axes */
/* Advise using CheckGoTo in external applications */
/* Return a flag indicating whether a slew is now in progress */
/*   1 -- slew is in progress on either drive */
/*   0 -- slew not in progress for either drive */

int GetSlewStatus(void)
{
  char sendstr[] = { 0x50, 0x01, 0x10, 0x13, 0x00, 0x00, 0x00, 0x01 };
  char returnstr[32];
  int numread;
    
  /* Query azimuth drive first */
  
  sendstr[2]=0x10;
  tcflush(TelPortFD,TCIOFLUSH);
  writen(TelPortFD,sendstr,8);
  numread=readn(TelPortFD,returnstr,2,2);
  if ( returnstr[0] == 0 ) 
  {
     return(1);
  }
  
  /* Query altitude drive if azimuth drive is not slewing */
  
  sendstr[2]=0x11;
  tcflush(TelPortFD,TCIOFLUSH);
  writen(TelPortFD,sendstr,8);
  numread=readn(TelPortFD,returnstr,2,2);
  if ( returnstr[0] == 0 ) 
  {
     return(1);
  }

 return(0);
}


/* Test whether the destination was reached                  */
/* Initiate the next segment if slewphase is greater than 1  */
/* Reset slewphase when goto has finished a segment          */
/* Return value is                                           */
/*   0 -- goto in progress                                   */
/*   1 -- goto complete within tolerance                     */
/*   2 -- goto complete but outside tolerance                */

int CheckGoTo(double desRA, double desDec, int pmodel)
{
  double telra1, teldec1;
  double errorRA, errorDec, nowRA, nowDec;
  double tolra, toldec;

  /* Is the telescope slewing? */
    
  if ( GetSlewStatus() == 1 )
  {
    /* One or more axes remain in motion */
    /* Try again later */
    
    return(0);
  }
  
  /* Was this a two-phase slew? */
  
  if ( slewphase == 2 )
  {
        
    /* Reset the slew phase and continue to the destination */
    
    slewphase = 0;    
    
    /* Go to the original destination */
    /* GoToCoords will change slewphase to 1 */
        
    GoToCoords(desRA, desDec, pmodel);
        
    /* Return a flag indicating a new goto operation is in progress */
    
    return(0);
  }
  else if ( slewphase == 1 )
  {    
      
    /* No axes are moving. Insure that tracking is started again. */
    
    StartTrack();
    
    /* Where are we now? */
  
    GetTel(&nowRA, &nowDec, pmodel);

    /* Compare to destination with pre-defined tolerances */
     
    telra1 = desRA;
    teldec1 = desDec;
        
    /* RA slew tolerance in hours */
    
    tolra = SLEWTOLRA;
    
    /* Dec slew tolerance in degrees */
    
    toldec = SLEWTOLDEC;

    /* What is the absolute value of the pointing error? */
  
    /* Magnitude of RA pointing error in hours */
    
    errorRA = fabs(nowRA - telra1);
    
    /* Magnitude of Dec pointing error in degrees */
    
    errorDec = fabs(nowDec - teldec1);
  
    /* Compare and notify whether we are within tolerance */

    if( ( errorRA > tolra ) || ( errorDec > toldec ) )
    {
      /* Result of slew is outside acceptable tolerance */
      /* Signal the calling routine that another goto may be needed */
    
      slewphase = 0;
      return(2);
    }
  }    
  else
  {
    /* Unexpected slew phase */
    /* Reset and return success without a test */
    /* This should clear errors and enable another slew request from the UI */
    /* Better would be to flag an error but that might have unintended consequences */
  
    slewphase = 0;    
  } 
  return(1); 
}

/* Coordinates and time */

/* Synchronize remote telescope to this ra-dec pair */
/* In this inteface simply update global offsets */

int SyncTelOffsets(double newoffsetra, double newoffsetdec)
{
  offsetra = newoffsetra;
  offsetdec = newoffsetdec;
  return 0;
}

/* Synchronize remote telescope to this UTC */

int SyncTelToUTC(double newutc)
{
  return 0;
}

/* Synchronize remote telescope to this observatory location  */

int SyncTelToLocation(double newlong, double newlat, double newalt)
{
  return 0;
}

/* Synchronize remote telescope to this local sidereal time  */

int SyncTelToLST(double newlst)
{
  return 0;
}

/* Start sidereal tracking                                                    */
/* StartTrack is aware of the latitude and will set the direction accordingly */
/* Call StartTrack at least once after the driver has the latitude            */

void StartTrack(void)
{
  
  char slewCmd[] = { 0x50, 0x03, 0x10, 0x06, 0xff, 0x0ff, 0x00, 0x00 };
  
  /* 0x50 is pass through code */
  /* 0x03 is number of data bytes including msgId */
  /* 0x10 is the destId for the ra drive, or 0x11 for declination */
  /* 0x06 is the msgId to set a positive drive rate */
  /* 0xff is the first of two data bytes for the sidereal drive rate */
  /* 0xff is the second of two data bytes for the sidereal drive rate */
  /* 0x00 is a null byte */
  /* 0x00 is a request to send no data back other than the # ack */

  char inputstr[2048];
  
  /* Test for southern hemisphere */
  /* Set negative drive rate if we're south of the equator */

  if ( SiteLatitude < 0. )
  {
    slewCmd[3] = 0x07;
  }
  
  tcflush(TelPortFD,TCIOFLUSH);
  
  writen(TelPortFD,slewCmd,8);

  /* Look for '#' acknowledgement of request */

  for (;;) 
  {
    if ( readn(TelPortFD,inputstr,1,1) ) 
    {
      if (inputstr[0] == '#')
      {
        break;
      } 
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from telescope sidereal track request\n");
    }
  }  

} 


/* Stop tracking if it is running */

void StopTrack(void)
{
  
  char slewCmd[] = { 0x50, 0x03, 0x10, 0x06, 0x00, 0x00, 0x00, 0x00 };
  
  /* 0x50 is pass through code */
  /* 0x03 is number of data bytes including msgId */
  /* 0x10 is the destId for the ra drive */
  /* 0x06 is the msgId to set a positive drive rate */
  /* 0x00 is the first of two data bytes for the drive rate */
  /* 0x00 is the second of two data bytes for the drive rate */
  /* 0x00 is a null byte */
  /* 0x00 is a request to send no data back other than the # ack */
    
  char inputstr[2048]; 
  
  /* Test for southern hemisphere */
  /* Set negative drive rate if we're south of the equator */

  if ( SiteLatitude < 0. )
  {
    slewCmd[3] = 0x07;
  }  

  tcflush(TelPortFD,TCIOFLUSH);

  writen(TelPortFD,slewCmd,8);

  /* Look for a '#' acknowledgement of request*/
  
  for (;;) 
  {
    if ( readn(TelPortFD,inputstr,1,1) ) 
    {
      if (inputstr[0] == '#')
      { 
        break;
      }  
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from telescope sidereal track off request\n");
    }
  }
}


/* Full stop */

void FullStop(void)

{  
  if( GetSlewStatus()==1 )
  {
    StopSlew(NORTH);
    usleep(250000.);
    StopSlew(SOUTH);
    usleep(250000.);
    StopSlew(EAST);
    usleep(250000.);
    StopSlew(WEST);
    usleep(250000.);
  }
  StopTrack();
}

/* Set slew limits control off or on */

int SetLimits(int limits)
{
  char inputstr[2048];
  int b0;
  char limitCmd[] = { 0x50, 0x02, 0x10, 0xef, 0x00, 0x00, 0x00, 0x00 };

  /* 0x50 is pass through code */
  /* 0x02 is number of data bytes including msgId */
  /* 0x10 is the destId for the RA drive */
  /* 0xef is the msgId to set hardstop limits state */
  /* 0x00 is the limits control data byte */
  /* 0x00 is the second null data byte */
  /* 0x00 is the third null data byte */
  /* 0x00 is a request to send no data back other than the # ack */
  
  if ( limits == TRUE )
  {
    limitCmd[4] = 0x01;  
    fprintf(stderr,"Limits enabled\n"); 
  }
  else
  {
    fprintf(stderr,"Limits disabled\n");   
  }
     
  /* Send the command */
  writen(TelPortFD,limitCmd,8);

  /* Wait for an acknowledgement */
  
  b0 = 1;
  
  for (;;) 
  {
    if ( readn(TelPortFD,inputstr,1,1) ) 
    {
      if ( inputstr[0] == '#' ) 
      {
        b0 = 0;
        break;
      }  
    }
  } 
  return (b0);
}


/* Get status of slew limits control */

int GetLimits(int *limits)
{
  char inputstr[2048];
  int b0, b1;
  char limitCmd[] = { 0x50, 0x01, 0x10, 0xee, 0x00, 0x00, 0x00, 0x01 };

  /* 0x50 is pass through code */
  /* 0x01 is number of data bytes including msgId */
  /* 0x10 is the destId for the RA drive */
  /* 0xee is the msgId to get hardstop limit state */
  /* 0x00 is the first null data byte */
  /* 0x00 is the second null data byte */
  /* 0x00 is the third null data byte */
  /* 0x01 is a request to send one byte data and # ack */
         
  /* Send the command */
  writen(TelPortFD,limitCmd,8);

  /* Read a response */
  readn(TelPortFD,inputstr,2,1);

  /* Mask the bytes */
  b0 = (0x000EF & inputstr[0]);
  if ( b0 == 1 )
  {
    *limits = 1;
  }
  else
  {
    *limits = 0;
  }

  if (inputstr[1] == '#')
  {
    b1 = 0;
  }
  else
  {
    b1 = 1;
  }
  return (b1);
}
  


/* Control the dew heater */

void Heater(int heatercmd)
{
}

/* Control the OTA fans */

void Fan(int fancmd)
{
  char fanCmd[] = { 0x50, 0x02, 0x13, 0x27, 0x00, 0x00, 0x00, 0x00 };
  char inputstr[2048];

  /* The CDK20 has one speed */
  /* This switch enables the fans without regard to speed */
  
  if ( fancmd > FANCMDOFF )
  {
    fanCmd[4] = 0x01;
  } 

  /* Send the command */
  writen(TelPortFD,fanCmd,8);

  /* Wait up to a second for an acknowledgement */
  
  for (;;) 
  {
    if ( readn(TelPortFD,inputstr,1,1) ) 
    {
      if (inputstr[0] == '#') break;
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from focus control\n");
    }
  }   
}


/* Adjust the focus */

void Focus(int focuscmd, int focusspd)
{
  char focusCmd[] = { 0x50, 0x02, 0x12, 0x24, 0x01, 0x00, 0x00, 0x00 };
  char inputstr[2048];
  static int focusflag;
  
  /* Set the speed */

  focusCmd[4] = 0x08;

  if ( focusspd >= FOCUSSPD4 )
  {
    focusCmd[4] = 0x08;
  }
  
  if ( focusspd == FOCUSSPD3 )
  {
    focusCmd[4] = 0x06;
  }
  
  if ( focusspd == FOCUSSPD2 )
  {
    focusCmd[4] = 0x04;
  }
  
  if ( focusspd == FOCUSSPD1 )
  {
    focusCmd[4] = 0x02;
  }
    
  if ( focuscmd == FOCUSCMDIN )
  {
    
    /* Run the focus motor in */
    
    focusflag = -1;

    /* Set the direction */
    
    if (focusdir > 0)
    {
      focusCmd[3] = 0x25;
    }
    else
    {
      focusCmd[3] = 0x24;
    }
            
    /* Send the command */
    writen(TelPortFD,focusCmd,8);

    /* Wait up to a second for an acknowledgement */
    
    for (;;) 
    {
      if ( readn(TelPortFD,inputstr,1,1) ) 
      {
        if (inputstr[0] == '#') break;
      }
      else 
      { 
        fprintf(stderr,"No acknowledgement from focus control\n");
      }
    }      
  }
  
  if ( focuscmd == FOCUSCMDOUT )
  {  
    
    /* Run the focus motor out */
    
    focusflag = 1;

    /* Set the direction */
    
    if (focusdir > 0)
    {
      focusCmd[3] = 0x24;
    }
    else
    {
      focusCmd[3] = 0x25;
    }
        
    /* Send the command */
    writen(TelPortFD,focusCmd,8);    

    /* Wait up to a second for an acknowledgement */
    
    for (;;) 
    {
      if ( readn(TelPortFD,inputstr,1,1) ) 
      {
        if (inputstr[0] == '#') break;
      }
      else 
      { 
        fprintf(stderr,"No acknowledgement from focus control\n");
      }
    }      
  }
   
  if ( focuscmd == FOCUSCMDOFF )
  {
    if (focusflag != 0)
    {

      /* Focus is in motion */      
      /* Set the speed to zero to stop the motion */
      
      focusCmd[4] = 0x00;
    
      /* Send the command */
      writen(TelPortFD,focusCmd,8);    

      /* Wait up to a second for an acknowledgement */
      
      for (;;) 
      {
        if ( readn(TelPortFD,inputstr,1,1) ) 
        {
          if (inputstr[0] == '#') break;
        }
        else 
        { 
        fprintf(stderr,"No acknowledgement from focus control\n");
        }
      }
      
      /* Reset focusflag */
         
      focusflag = 0;
      
    }  
    if ( focusflag == 0 )
    {
      
      /* Focus motion is flagged as stopped so there should be nothing to do */
    
    }  
  }    
}


/* Report the current focus reading */

void GetFocus(double *telfocus)
{

  /* Focus motor device ID 0x12 */
  /* MC_GET_POSITION is 0x01 */
  /* Bytes in response are 0x03 */
  
  char focusCmd[] = { 0x50, 0x01, 0x12, 0x01, 0x00, 0x00, 0x00, 0x03 };
  char returnstr[2048];  
  int b0,b1,b2;
  int count;
  double focus;
    
  /* Send the command */
  
  writen(TelPortFD,focusCmd,8);    
  
  /* Read a response */

  readn(TelPortFD,returnstr,4,1);

  b0 = (unsigned char) returnstr[0];
  b1 = (unsigned char) returnstr[1];
  b2 = (unsigned char) returnstr[2];
  
  /* These counts roll over to 255 when the counter goes negative. */
  /* That is, 256*256*255 + 256*255 + 255 is -1 */
    
  count = 256*256*b0 + 256*b1 + b2;
  
  /* Use a scale which goes negative below zero */
  
  if (count > 8388608)
  {
    count = count - 16777217;
  }
    
  focus = count;
  
  /* Apply a conversion so that the focus scale comes out in decimal microns. */
  /* The variable focusscale is defined globally. */
   
  focus = focus/focusscale;     
  *telfocus = focus;
  
}



/* Adjust the rotation */

void Rotate(int rotatecmd, int rotatespd)
{
}

/* Report the rotation setting */

void GetRotate(double *telrotate)
{
}

/* Report the temperature */

void GetTemperature(double *teltemperature)
{
  
  char focusCmd[] = { 0x50, 0x02, 0x12, 0x26, 0x00, 0x00, 0x00, 0x02 };
  char returnstr[2048];  
  int b0,b1;
  int count;
  double value;
    
  value = 0.;
    
  /* Send the command */
  
  writen(TelPortFD,focusCmd,8);    
  
  /* Read a response */

  readn(TelPortFD,returnstr,3,1);

  b0 = (unsigned char) returnstr[0];
  b1 = (unsigned char) returnstr[1];
  
  /* In the C20 the counts are left shifted by 4 bits */
  /* The total count comes in increments of 16 for a 12 bit sensor */

  /* These counts roll over to 255 when the counter goes negative. */
  /* That is, 256*255 + 255 is -1 */
  
  count = 256*b0 + b1;

  /* Use a scale which goes negative below zero */
  
  if (count > 32768)
  {
    count = count - 65537;
  }

  /* Apply the calibration for the Maxim DS18B20 sensor in the OTA */
  /* Right shift the bits to match the specification calibration */
  
  value = count/16;
  value = 0.0625 * value;
  
  /* Test for out of range as an indicator of sensor not present */
  /* Set an out of range value that is not annoying in a display */
  
  if ((value < -50 ) || (value > 50) )
  {
    value = 0.;
  }  
 
  *teltemperature = value;
  return;

}


/* Time synchronization utilities */

/* Reset the telescope sidereal time */

int  SyncLST(double newTime)
{		
  fprintf(stderr,"AUX mode of NexStar uses computer time and site location only \n");
  return -1;
}


/*  Reset the telescope local time */

int  SyncLocalTime()
{
  fprintf(stderr,"AUX mode of NexStar uses computer time only \n");
  return -1;
}


/* Serial port utilities */

static int writen(fd, ptr, nbytes)
int fd;
char *ptr;
int nbytes;
{
  int nleft, nwritten;
  nleft = nbytes;
  while (nleft > 0) 
  {
    nwritten = write (fd, ptr, nleft);
    if (nwritten <=0 ) break;
    nleft -= nwritten;
    ptr += nwritten;
  }
  return (nbytes - nleft);
}

static int readn(fd, ptr, nbytes, sec)
int fd;
char *ptr;
int nbytes;
int sec;
{
  int stat;
  int nleft, nread;
  nleft = nbytes;
  while (nleft > 0) 
  {
    stat = telstat(fd,sec,0);
    if (stat <=  0 ) break;
    nread  = read (fd, ptr, nleft);
    if (nread <= 0)  break;
    nleft -= nread;
    ptr += nread;
  }
  return (nbytes - nleft);
}

/* Examines the read status of a file descriptor.                       */
/* The timeout (sec, usec) specifies a maximum interval to              */
/* wait for data to be available in the descriptor.                     */
/* To effect a poll, the timeout (sec, usec) should be 0.               */
/* Returns non-negative value on data available.                        */
/* 0 indicates that the time limit referred by timeout expired.         */
/* On failure, it returns -1 and errno is set to indicate the error.    */

static int telstat(fd,sec,usec)
register int fd, sec, usec;
{
  int ret;
  int width;
  struct timeval timeout;
  telfds readfds;

  memset((char *)&readfds,0,sizeof(readfds));
  FD_SET(fd, &readfds);
  width = fd+1;
  timeout.tv_sec = sec;
  timeout.tv_usec = usec;
  ret = select(width,&readfds,NULL_PTR(telfds),NULL_PTR(telfds),&timeout);
  return(ret);
}

