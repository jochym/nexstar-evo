/* ---------------------------------------------------------------------------*/
/* -                Protocol for NexStar telescope control                   -*/
/* ---------------------------------------------------------------------------*/
/*                                                                            */
/* Copyright 2006-2012 John Kielkopf                                          */
/*                                                                            */
/* Distributed under the terms of the General Public License (see LICENSE)    */
/*                                                                            */
/* John Kielkopf (kielkopf@louisville.edu)                                    */
/*                                                                            */
/* Date: July 22, 2012                                                        */
/* Version: 6.0                                                               */
/*                                                                            */
/* History:                                                                   */
/*                                                                            */
/* April 5, 2006                                                              */
/*   Version 3.0                                                              */
/*                                                                            */
/*   Updated for compatibility with XmTel 3.0                                 */
/*     Tested on                                                              */
/*     Celestron GPS 11-inch fork-mounted Schmidt-Cassegrain                  */
/*     Celestron CGE 11-inch German equatorial Schmidt-Cassegrain             */
/*     Lightwave CDK20 on a prototype Celestron large CGE mount               */
/*                                                                            */
/* September 8, 2006                                                          */
/*   Version 3.0                                                              */
/*                                                                            */
/*   Implemented focus motor control for CDK20                                */
/*                                                                            */
/* May 20, 2007                                                               */
/*   Version 4.0                                                              */
/*                                                                            */
/*   Modified routines for compatibility with remote version of XmTel         */
/*   Corrected errors in AUX command set usage for start/stop track           */
/*   Corrected error in CDK20 focus count evaluation                          */
/*   Corrected error in AUX command to start southern hemisphere motion       */
/*                                                                            */
/* June 23, 2007                                                              */
/*   Version 4.01                                                             */
/*                                                                            */
/*   SiteLatitude external value used to flag southern hemisphere state       */
/*                                                                            */
/* April 10, 2008                                                             */
/*   Version 4.10                                                             */
/*                                                                            */
/*   Slew limit control added for CDK20                                       */
/*                                                                            */
/* August 18, 2008                                                            */
/*   Version 4.10                                                             */
/*                                                                            */
/*   Removed SyncTelToCoords                                                  */
/*   Offsets are now always handled in the pointing model                     */
/*                                                                            */
/* September 7, 2008                                                          */
/*   Version 5.0 for INDI released                                            */
/*                                                                            */
/* July 27, 2009                                                              */
/*   Added configuration entry for home position                              */
/*                                                                            */
/* November 10, 2009                                                          */
/*   Corrected error in focus count                                           */
/*                                                                            */
/* June 25, 2010                                                              */
/*   Version 5.1                                                              */
/*   Removed unused timer count from focus encoder routine                    */
/*   Added fan control from aux version                                       */
/*   Removed unused time sync functions                                       */
/*   Added device option for serial port                                      */
/*                                                                            */
/* October 24, 2010                                                           */
/*   Version 5.2                                                              */
/*   Focus, rotate, and temperature now system calls                          */
/*   Lowered rate for slew to 8 as recommened by Dan Holler                   */
/*                                                                            */
/* October 10, 2011                                                           */
/*   Version 5.3                                                              */
/*   Added CenterGuide for compatibility with current xmtel                   */
/*                                                                            */
/* January 29, 2012                                                           */
/*   Version 5.4                                                              */
/*   Compatible with latest xmtel                                             */
/*                                                                            */
/* July 1, 2012                                                               */
/*   Version 6.0                                                              */
/*   Leapsecond incremented to 35.0                                           */


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

/* There are two classes of routines defined here:                    */

/*   XmTel commands to allow easy NexStar access.  These              */
/*     include routines that mimic the extensive LX200 command        */
/*     language and, for the most part, trap calls and                */
/*     respond with an error message to the console.                  */

/*   NexStar specific commands and data.                              */

/*     This version of xmtel uses a few                               */
/*     auxilliary commands which permit direct access to the motor    */
/*     controllers.                                                   */


/* System variables and prototypes */

/* Telescope and mounting commands that may be called externally */

/* Interface control */

void ConnectTel(void);
void DisconnectTel(void);
int  CheckConnectTel(void);

/* Slew and track control */

void SetRate(int newRate);
void StartSlew(int direction);
void StopSlew(int direction);
void StartTrack(void);
void CenterGuide(double centerra, double centerdec, 
  int raflag, int decflag, int pmodel);
void StopTrack(void);
void FullStop(void);

/* Coordinates and time */

void GetTel(double *telra, double *teldec, int pmodel);
int  GoToCoords(double newRA, double newDec, int pmodel);
int  CheckGoTo(double desRA, double desDec, int pmodel);

/* Slew limits */

int  GetSlewStatus(void);
int  SetLimits(int limits);
int  GetLimits(int *limits);

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
extern double offsetra, offsetdec;
extern double SiteLatitude;
extern double homeha;            /* Home ha                 */
extern double homera;            /* Home ra derived from ha */
extern double homedec;           /* Home dec                */ 
extern char   telserial[32];     /* Serial port             */

extern void PointingFromTel (double *telra1, double *teldec1, 
  double telra0, double teldec0, int pmodel);

extern void PointingToTel (double *telra0, double *teldec0, 
  double telra1, double teldec1, int pmodel);
  
/* NexStar local data */

static int slewRate;        /* Rate for slew request in StartSlew */

/* Files */

FILE *fp_focus;
char *focusfile;

FILE *fp_temperature;
char *temperaturefile;

FILE *fp_rotate;
char *rotatefile;


/* Communications variables and routines for internal use */

static int TelPortFD;
static int TelConnectFlag = FALSE;

typedef fd_set telfds;

static int readn(int fd, char *ptr, int nbytes, int sec);
static int writen(int fd, char *ptr, int nbytes);
static int telstat(int fd,int sec,int usec);

/* End of prototype and variable definitions */

/*
 *
 * Handcontroller interfacing notes 
 *
 * The command format for a connection through the hand controller is    
 * 
 *   where
 *
 *   0x50 is a code requesting that the data pass through to the motors     
 *   msgLen is how many bytes including msgId  and valid data bytes     
 *   destId is the AUX command set destination number                    
 *   msgId  is the AUX command set message number                        
 *   data1-3 are up to 3 bytes of message data                           
 *   responseBytes is the number of bytes to echo back in response       
 *
 * For example, the command to get data from the focus motor is          
 *
 *   focusCmd[] = { 0x50, 0x01, 0x12, 0x01, 0x00, 0x00, 0x00, 0x03 }     
 *
 * These pass-through commands are used in                               
 *
 *   StartTrack 
 *   StopTrack  
 *   StartSlew  
 *   StopSlew   
 *
 */
 


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
  char returnStr[128];
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

  /* Test connection */

  writen(TelPortFD,"Kx",2);
  numRead=readn(TelPortFD,returnStr,3,2);
  returnStr[numRead] = '\0';

  if (numRead == 2) 
  {
    TelConnectFlag = TRUE;
  }  

  /* Perform startup tests */

  flag = GetLimits(&limits);
  printf("Reading old limits flag %d and return value  %d\n",limits, flag);

  usleep(1000000);

  limits = FALSE;
  flag = SetLimits(limits);
  printf("Setting new limits flag %d and return value  %d\n",limits, flag);
  
  usleep(1000000);
  
  flag = GetLimits(&limits);
  printf("Reading new limits flag %d and return value  %d\n",limits, flag);
   
  

  /* Diagnostic tests */

/*  
  printf("ConnectTel read %d characters: %s\n",numRead,returnStr); 
  printf("TelConnectFlag set to: %d\n",TelConnectFlag); 
*/

}

/* Assign and save slewRate for use in StartSlew */
/* SLEW rate lowered to 8 instead of 9 to provide more control */

void SetRate(int newRate)
{
  if(newRate == SLEW) 
    {
      slewRate = 8; 
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
  char inputStr[2048];
  
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
    if ( readn(TelPortFD,inputStr,1,1) ) 
    {
      if (inputStr[0] == '#') break;
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from telescope in StartSlew.\n");
    }
  }   
}


/* Stop the slew in chosen direction */

void StopSlew(int direction)
{
  char slewCmd[] = { 0x50, 0x02, 0x11, 0x24, 0x00, 0x00, 0x00, 0x00 };
  char inputStr[2048];
  
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

  writen(TelPortFD,slewCmd,8);

  /* Look for '#' acknowledgement of request*/

  for (;;) 
  {
    if ( readn(TelPortFD,inputStr,1,1) ) 
    {
      if (inputStr[0] == '#') break;
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from telescope in StopSlew.\n");
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


/* Find the coordinates at which the telescope is pointing */
/* Correct for the pointing model */

void GetTel(double *telra, double *teldec, int pmodel)
{
  char returnStr[12];
  int countRA,countDec;
  double telra0, teldec0, telra1, teldec1;
  int numRead;

  writen(TelPortFD,"E",1);
  numRead=readn(TelPortFD,returnStr,10,1);
  returnStr[4] = returnStr[9] = '\0';  

/* Diagnostic
 *
 * printf("GetRAandDec: %d read %x\n",numRead,returnStr); 
 *
 */
 
  sscanf(returnStr,"%x",&countRA);                  
  sscanf(returnStr+5,"%x:",&countDec);               
  telra0 = (double) countRA;
  telra0 = telra0 / (3. * 15. * 60. * 65536./64800.);
  teldec0 = (double) countDec;
  teldec0 = teldec0 / (3. * 60. * 65536./64800.);


/* Account for the quadrant in declination */

/* 90 to 180 */

  if ( (teldec0 > 90.) && (teldec0 <= 180.) )
  {  
    teldec0 = 180. - teldec0; 
  }


/* 180 to 270 */

  if ( (teldec0 > 180.) && (teldec0 <= 270.) )
  {
    teldec0 = teldec0 - 270.;
  }


/* 270 to 360 */

  if ( (teldec0 > 270.) && (teldec0 <= 360.) )
  {
    teldec0 = teldec0 - 360.;
  }

  /* Correct the coordinates that are reported by the telescope */
  
  PointingFromTel(&telra1, &teldec1, telra0, teldec0, pmodel);
 
  /* Return corrected values */

  *telra=telra1;
  *teldec=teldec1;

  return;

}


/* Slew to new coordinates */
/* Returns 1 if goto command is sent successfully and 0 otherwise*/

int GoToCoords(double newRA, double newDec, int pmodel)
{
  double telra0, telra1, teldec0, teldec1;
  int countRA,countDec;
  char r0,r1,r2,r3,d0,d1,d2,d3;
  double degs, hrs;
  char outputStr[32], inputStr[2048];

  /* Where do we really want to go to? */
  
  telra1 = newRA;
  teldec1 = newDec;
  
  /* Where will the telescope point to make this happen? */
  
  PointingToTel(&telra0,&teldec0,telra1,teldec1,pmodel);
  
  /* Work with the telescope coordinates */
  
  newRA = telra0;
  newDec = teldec0;

  /* Set coordinates */

  hrs = newRA;
  degs = newDec;  

  /* Convert float RA to integer count */

  hrs = hrs*(3. * 15. * 60. * 65536./64800.);
  countRA = (int) hrs;
  

/* Account for the quadrant in declination */

  if ( (newDec >= 0.0) && (newDec <= 90.0) )
  {
    degs = degs*(3. * 60. * 65536./64800.);
  }
  else if ( (newDec < 0.0) && (newDec >= -90.0) )
  {
    degs = (360. + degs)*(3. * 60. * 65536./64800.);
  }
  else
  {
    fprintf(stderr,"Invalid newDec in GoToCoords.\n");
    return 0;  
  }



  /* Convert float Declination to integer count */

  countDec = (int) degs;


  /* Convert each integer count to four HEX characters */
  /* Inline coding just to be fast */
  

  if(countRA < 65536)
  {
    r0 = countRA % 16;
    if(r0 < 10)
    {
      r0 = r0 + 48;
    }
    else
    {
      r0 = r0 + 55;
    }
    countRA = countRA/16;
    r1 = countRA % 16;
    if(r1 < 10)
    {
      r1 = r1 + 48;
    }
    else
    {
      r1 = r1 + 55;
    }    
    countRA = countRA/16;
    r2 = countRA % 16;
    if(r2 < 10)
    {
      r2 = r2 + 48;
    }
    else
    {
      r2 = r2 + 55;
    }    
    r3 = countRA/16;
    if(r3 < 10)
    {
      r3 = r3 + 48;
    }
    else
    {
      r3 = r3 + 55;
    }    
  }
  else
  {
    printf("RA count overflow in GoToCoords.\n");
    return 0;
  }
   if(countDec < 65536)
  {
    d0 = countDec % 16;
    if(d0 < 10)
    {
      d0 = d0 + 48;
    }
    else
    {
      d0 = d0 + 55;
    }    
    countDec = countDec/16;
    d1 = countDec % 16;
    if(d1 < 10)
    {
      d1 = d1 + 48;
    }
    else
    {
      d1 = d1 + 55;
    }    
    countDec = countDec/16;
    d2 = countDec % 16;
    if(d2 < 10)
    {
      d2 = d2 + 48;
    }
    else
    {
      d2 = d2 + 55;
    }
    d3 = countDec/16;
    if(d3 < 10)
    {
      d3 = d3 + 48;
    }
    else
    {
      d3 = d3 + 55;
    }    
  }
  else
  {
    fprintf(stderr,"Dec count overflow in GoToCoords.\n");
    return 0;
  }
    
  
  /* Send the command and characters to the NexStar */

  sprintf(outputStr,"R%c%c%c%c,%c%c%c%c",r3,r2,r1,r0,d3,d2,d1,d0);
  writen(TelPortFD,outputStr,10);

  /* Look for '#' in response */
  
  for (;;) 
  {
    if ( readn(TelPortFD,inputStr,1,2) ) 
    {
      if (inputStr[0] == '#') break;
    }
    else 
    fprintf(stderr,"No acknowledgement from telescope after GoToCoords.\n");
    return 0;
  }
  return 1;
}


/* Return a flag indicating whether a slew is now in progress */
/*   1 -- slew is in progress     */
/*   0 -- slew not in progress    */

int GetSlewStatus(void)
{
  char inputStr[2048];
  writen(TelPortFD,"L",1);

  /* Look for '0#' in response indicating goto is not in progress */
  
  for (;;) 
  {
    if ( readn(TelPortFD,inputStr,2,2) ) 
    {
      if ( (inputStr[0] == '0') &&  (inputStr[1] == '#')) break;
    }
    else 
    return 1;
  }
  return 0;
}


/* Test whether the destination was reached */
/* With the NexStar we use the goto in progress query */
/* Return value is  */
/*   0 -- goto in progress */
/*   1 -- goto complete within tolerance */
/*   2 -- goto complete but outside tolerance */

int CheckGoTo(double desRA, double desDec, int pmodel)
{
  double telra0, telra1, teldec0, teldec1;
  double errorRA, errorDec, nowRA, nowDec;

  /* Where do we really want to go to? */
  
  telra1 = desRA;
  teldec1 = desDec;
  
  /* Where will the telescope point to make this happen? */
  
  PointingToTel(&telra0,&teldec0,telra1,teldec1,pmodel);
  
  /* Work with the telescope coordinates */
  
  desRA = telra0;
  desDec = teldec0;
  
  if ( GetSlewStatus() == 1 )
  {
    return (0);
  }
  
  GetTel(&nowRA, &nowDec, pmodel);
  errorRA = nowRA - desRA;
  errorDec = nowDec - desDec;

  /* For 6 minute of arc precision; change as needed.  */

  if( fabs(errorRA) > (0.1/15.) || fabs(errorDec) > 0.1)
  {
    return (2);
  }
  else
  {
    return (1);
  }
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

  char inputStr[2048];
  
  /* Test for southern hemisphere */
  /* Set negative drive rate if we're south of the equator */

  if ( SiteLatitude < 0. )
  {
    slewCmd[3] = 0x07;
  }
  
  writen(TelPortFD,slewCmd,8);

  /* Look for '#' acknowledgement of request */

  for (;;) 
  {
    if ( readn(TelPortFD,inputStr,1,1) ) 
    {
      if (inputStr[0] == '#') break;
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from telescope in StartTrack.\n");
    }
  }  

} 

/* Use high resolution encoder to improve tracking */

void CenterGuide(double centerra, double centerdec, 
  int raflag, int decflag, int pmodel)
{
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
    
  char inputStr[2048]; 
  
  /* Test for southern hemisphere */
  /* Set negative drive rate if we're south of the equator */

  if ( SiteLatitude < 0. )
  {
    slewCmd[3] = 0x07;
  }  

  writen(TelPortFD,slewCmd,8);

  /* Look for a '#' acknowledgement of request*/
  
  for (;;) 
  {
    if ( readn(TelPortFD,inputStr,1,1) ) 
    {
      if (inputStr[0] == '#') break;
    }
    else 
    { 
      fprintf(stderr,"No acknowledgement from telescope in StopTrack.\n");
    }
  }
}


/* Full stop */

void FullStop(void)

{  
  if( GetSlewStatus()==1 )
  {
    printf("NexStar FullStop: stopping N slew.\n");
    StopSlew(NORTH);
    printf("NexStar FullStop: stopping S slew.\n");
    StopSlew(SOUTH);
    printf("NexStar FullStop: stopping E slew.\n");
    StopSlew(EAST);
    printf("NexStar FullStop: stopping W slew.\n");
    StopSlew(WEST);
    usleep(1000000.);
  }
  printf("NexStar: stopping sidereal tracking.\n");
  StopTrack();
}

/* Set slew limits control off or on */

int SetLimits(int limits)
{
  char inputStr[2048];
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
    if ( readn(TelPortFD,inputStr,1,1) ) 
    {
      if ( inputStr[0] == '#' ) 
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
  char inputStr[2048];
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
  readn(TelPortFD,inputStr,2,1);

  /* Mask the bytes */
  b0 = (0x000EF & inputStr[0]);
  if ( b0 == 1 )
  {
    *limits = 1;
  }
  else
  {
    *limits = 0;
  }

  if (inputStr[1] == '#')
  {
    b1 = 0;
  }
  else
  {
    b1 = 1;
  }
  return (b1);
}
  

/* Set slew speed limited by MAXSLEWRATE */

int SetSlewRate(int slewRate)
{
  fprintf(stderr,"NexStar does not support remote setting of slew rate.\n");
  return 0;
}


/* Control the dew and drive heaters */

void Heater(int heatercmd)
{
  char cmdstr[256];
  sprintf(cmdstr,"setheater %d 1>/dev/null 2>/dev/null", heatercmd);
  system(cmdstr);  
}

/* Control the telescope fans */

void Fan(int fancmd)
{
  char cmdstr[256];
  sprintf(cmdstr,"setfan %d 1>/dev/null 2>/dev/null", fancmd);
  system(cmdstr);   
}

/* Adjust the focus using an external routine */
/* The routine will time out on its own and report through the status file */

void Focus(int focuscmd, int focusspd)
{
  char cmdstr[256];
  sprintf(cmdstr,"setfocus %d %d 1>/dev/null 2>/dev/null", focuscmd, focusspd);
  system(cmdstr);  
}


/* Report the current focus reading from the status file */

void GetFocus(double *telfocus)
{
  int nread;
  double current_focus;
  char cmdstr[256];
  sprintf(cmdstr,"getfocus 1>/dev/null 2>/dev/null");
  system(cmdstr);  
  
  focusfile = (char*) malloc (MAXPATHLEN);
  strcpy(focusfile,FOCUSFILE);
  fp_focus = fopen(focusfile, "r");
  if (fp_focus == NULL)
  {
    return;
  }
  
  nread = fscanf(fp_focus, "%lg", &current_focus);
  fclose(fp_focus);
  
  if (nread !=0)
  {
    *telfocus = current_focus;
  }
  
  return;  
}



/* Adjust the rotation */

void Rotate(int rotatecmd, int rotatespd)
{
  char cmdstr[256];
  sprintf(cmdstr,"setrotate %d %d  1>/dev/null 2>/dev/null", rotatecmd, rotatespd);
  system(cmdstr);
}

/* Report the rotation setting */

void GetRotate(double *telrotate)
{
  int nread;
  double current_rotate;
  char cmdstr[256];
  sprintf(cmdstr,"getrotate 1>/dev/null 2>/dev/null");
  system(cmdstr);  
  
  rotatefile = (char*) malloc (MAXPATHLEN);
  strcpy(rotatefile,ROTATEFILE);
  fp_rotate = fopen(rotatefile, "r");
  if (fp_rotate == NULL)
  {
    return;
  }
  
  nread = fscanf(fp_rotate, "%lg", &current_rotate);
  fclose(fp_rotate);
  
  if (nread !=0)
  {
    *telrotate = current_rotate;
  }
  
  return;  
}

/* Report the temperature */

void GetTemperature(double *teltemperature)
{
  int nread;
  double current_temperature;
  char cmdstr[256];
  
  sprintf(cmdstr,"gettemperature 1>/dev/null 2>/dev/null");
  system(cmdstr);  
  
  temperaturefile = (char*) malloc (MAXPATHLEN);
  strcpy(temperaturefile,TEMPERATUREFILE);
  fp_temperature = fopen(temperaturefile, "r");
  if (fp_temperature == NULL)
  {
    return;
  }
  
  nread = fscanf(fp_temperature, "%lf", &current_temperature);
  fclose(fp_temperature);
  
  if (nread !=0)
  {
    *teltemperature = current_temperature;
  }
  
  return;
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

/*  Diagnostic */    
    
/*    printf("readn: %d read\n", nread);  */

    if (nread <= 0)  break;
    nleft -= nread;
    ptr += nread;
  }
  return (nbytes - nleft);
}

/*
 * Examines the read status of a file descriptor.
 * The timeout (sec, usec) specifies a maximum interval to
 * wait for data to be available in the descriptor.
 * To effect a poll, the timeout (sec, usec) should be 0.
 * Returns non-negative value on data available.
 * 0 indicates that the time limit referred by timeout expired.
 * On failure, it returns -1 and errno is set to indicate the
 * error.
 */
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

