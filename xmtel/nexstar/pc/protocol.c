/* ---------------------------------------------------------------------------*/
/* -    Protocol for PC port control of NexStar telescope motor boards       -*/
/* ---------------------------------------------------------------------------*/
/*                                                                            */
/* Copyright 2008 John Kielkopf                                               */
/*                                                                            */
/* Distributed under the terms of the General Public License (see LICENSE)    */
/*                                                                            */
/* John Kielkopf (kielkopf@louisville.edu)                                    */
/*                                                                            */
/* Date: November 4, 2008                                                     */
/* Version: 1.0                                                               */
/*                                                                            */
/* History:                                                                   */
/*                                                                            */
/* November 11, 2008                                                           */
/*   Version 1.0                                                              */
/*                                                                            */
/*   Derived from XmTel 5.0.2 for NexStar HC version 5.0                      */


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
extern double offsetra, offsetdec;
extern double SiteLatitude;

extern void PointingFromTel (double *telra1, double *teldec1, 
  double telra0, double teldec0, int pmodel);

extern void PointingToTel (double *telra0, double *teldec0, 
  double telra1, double teldec1, int pmodel);
  

/* NexStar local data */

static int slewRate;        /* Rate for slew request in StartSlew */
static int focuscount;      /* Used to keep a focus position count */

/* Communications variables and routines for internal use */

static int TelPortFD;
static int TelConnectFlag = FALSE;

typedef fd_set telfds;

static int readn(int fd, void *ptr, int nbytes, int sec);
static int writen(int fd, void *ptr, int nbytes);
static int telstat(int fd,int sec,int usec);
void checksum(unsigned char* packet, int start, int stop);

/* End of prototype and variable definitions */


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


/* Connect to the Celestron mounting PC serial interface */
/* Returns without action if TelConnectFlag is TRUE */
/* Sets TelConnectFlag TRUE on success */

void ConnectTel(void)
{
  struct termios tty;
  /* unsigned char sendStr[] = { 0x3b, 0x03, 0x05, 0x11, 0xfe, 0xeb }; */
  
  unsigned char sendStr[] = { 0x3b, 0x03, 0x05, 0x10, 0xfe, 0x00 };
      
  /* Packet format:        */
  /*   preamble            */
  /*   packet length       */
  /*   source (0x05)       */
  /*   destination         */
  /*   message id          */
  /*   message data bytes  */
  /*   checksum byte       */
  
  unsigned char returnStr[128];
  int ackRead;
  int limits, flag;
  int i;
  
  printf("Requesting port \n");


  if(TelConnectFlag != FALSE)
    return;

  /* Make the connection             */
  /* The NexStar PC port uses --     */
  /*   19200 bits per second         */
  /*   no parity                     */
  /*   1 stop bit                    */
  /*   hardware RTS/CTS flow control */

  TelPortFD = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY );
  if(TelPortFD == -1)
  {
    return;
  }
  
  
  printf("Port opened \n");
  
  tcgetattr(TelPortFD,&tty);
    
  /* Set the baud rate to 19200 */
  
  cfsetospeed(&tty, (speed_t) B19200);
  cfsetispeed(&tty, (speed_t) B19200);

  /* Enable the receiver and set local mode */
  
  tty.c_cflag |= (CLOCAL | CREAD);
  
  /* Mask character size to 8 bits and no parity (8N1) */
  
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;

  /* Enable hardware flow control */
  
  tty.c_cflag |= CRTSCTS;
    
  /* Set canonical raw input */
  
  tty.c_lflag &= ~(ICANON | ECHO | ISIG);
  
  /* Set output processing for raw data */
  
  tty.c_oflag &= ~OPOST;

  /* Set the new options for the port */
  
  tcsetattr(TelPortFD, TCSANOW, &tty);
  
  fprintf(stderr, "Options are set \n");

  /* Flush the input (read) buffer */

  tcflush(TelPortFD,TCIOFLUSH);

  /* Test connection by asking for the firmware version */
  /* Response for our C20 will be 53.51 for either axis */
  
  /* Set the checksum byte */
      
  checksum(sendStr,1,4);
  fprintf(stderr,"Checksum 0x%02x\n",sendStr[5]);
  
  fprintf(stderr, "Sending query \n");
  
  for (i = 0; i < 6; i++)
  {
    fprintf(stderr, "<0x%02x> ", sendStr[i]);
  }
  fprintf(stderr, "\n");
          
  /* Send the string to the controller */
    
  /* writen(TelPortFD,sendStr,6); */
  
  int nout;
  
  nout = write(TelPortFD,sendStr,6);
  
  fprintf(stderr, "%d characters written\n", nout);
  
  /* Read its echo and data from the controller */
  
  unsigned char chin;
  int nin;
  
  while (1)
  {
    /* read(TelPortFD, &chout, sizeof(chout)); */
             
    nin = read(TelPortFD, &chin, 1);
    
    if (nin == 1)
       fprintf(stderr,"<0x%02x> ", chin);

    usleep(20000);
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
/*   3 -- goto complete but outside tolerance */

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
    return 0;
  }
  
  GetTel(&nowRA, &nowDec, pmodel);
  errorRA = nowRA - desRA;
  errorDec = nowDec - desDec;

  /* For 6 minute of arc precision; change as needed.  */

  if( fabs(errorRA) > (0.1/15.) || fabs(errorDec) > 0.1)
    return 1;
  else
    return 2;
}


/* Coordinates and time */

/* Synchronize remote telescope to this ra dec pair */

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
/* Overrides UTC and location when implemented */

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
  char returnStr[128];
  int b0, b1;
  char sendStr[] = { 0x3b, 0x03, 0x0d, 0x10, 0xee, 0x00 };

  /* 0x10 is the destId for the RA drive */
  /* 0xee is the msgId to get hardstop limit state */
         
  /* sendStr[5] = checksum(sendStr,3); */
  
  /* Send the command */
  writen(TelPortFD,sendStr,6);

  /* Read a response */
  readn(TelPortFD,returnStr,8,1);

  /* Mask the bytes */
  b0 = (0x000EF & returnStr[6]);
  if ( b0 == 1 )
  {
    *limits = 1;
  }
  else
  {
    *limits = 0;
  }

  if (returnStr[7] == '#')
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


/* Control the dew heater */

void Heater(int heatercmd)
{
}

/* Control the fan */

void Fan(int fancmd)
{
}

/* Adjust the focus */

void Focus(int focuscmd, int focusspd)
{
  char focusCmd[] = { 0x50, 0x02, 0x12, 0x24, 0x01, 0x00, 0x00, 0x00 };
  char inputStr[2048];
  static int focusflag;
  static double focustime;
  
  
  if ( focuscmd == FOCUSCMDIN )
  {
    
    /* Run the focus motor in */
    
    focusflag = -1;
    focustime = UTNow();

    /* Set the direction */
    focusCmd[3] = 0x25;
    
    /* Set the speed  but enforce the speed limit first */
    if ( focusspd > FOCUSSPDMAX )
    {
      focusCmd[4] = 0x08;
    }
    else if ( focusspd == FOCUSSPDFAST )
    {
      focusCmd[4] = 0x04;
    }
    else if (focusspd == FOCUSSPDSLOW )
    {
      focusCmd[4] = 0x02;
    }
    else if ( focusspd == FOCUSSPDMIN )
    {
      focusCmd[4] = 0x01;
    }
    else 
    {
      focusCmd[4] = 0x01;
    }
        
    /* Send the command */
    writen(TelPortFD,focusCmd,8);

    /* Wait up to a second for an acknowledgement */
    for (;;) 
    {
      if ( readn(TelPortFD,inputStr,1,1) ) 
      {
        if (inputStr[0] == '#') break;
      }
      else 
      { 
        fprintf(stderr,"No acknowledgement from focus in.\n");
      }
    }      
  }
  
  if ( focuscmd == FOCUSCMDOUT )
  {  
    
    /* Run the focus motor out */
    
    focusflag = 1;
    focustime = UTNow();

    /* Set the direction */
    focusCmd[3] = 0x24;
    
    /* Set the speed  but enforce the speed limit first */
    if ( focusspd > FOCUSSPDMAX )
    {
      focusspd = FOCUSSPDMAX;    
    }

    focusCmd[4] = (char) focusspd;
        
    /* Send the command */
    writen(TelPortFD,focusCmd,8);    

    /* Wait up to a second for an acknowledgement */
    for (;;) 
    {
      if ( readn(TelPortFD,inputStr,1,1) ) 
      {
        if (inputStr[0] == '#') break;
      }
      else 
      { 
        fprintf(stderr,"No acknowledgement from focus out.\n");
      }
    }      
  }
   
  
  if ( focuscmd == FOCUSCMDOFF )
  {
    if (focusflag != 0)
    {

      /* Focus is in motion so we note the time and then stop it */
      /* Keep track of count as number of seconds at slowest speed */
      
      focustime = UTNow() - focustime;
      
      /* Set the speed to zero to stop the motion */
      focusCmd[4] = 0x00;
    
      /* Send the command */
      writen(TelPortFD,focusCmd,8);    

      /* Wait up to a second for an acknowledgement */
      for (;;) 
      {
        if ( readn(TelPortFD,inputStr,1,1) ) 
        {
          if (inputStr[0] == '#') break;
        }
        else 
        { 
        fprintf(stderr,"No acknowledgement from focus stop.\n");
        }
      }   
      
      /* Just in case the UT clock rolled over one day while focusing */
      /* Try to trap the error but ...                                */
      /*   for very short focustime roundoff error may give negative  */
      /*   values instead of zero.  We trap those cases.              */
      
      if ( focustime < 0. )
      {
        if ( focustime < -23.9 )
        {
          focustime = focustime + 24.;
        }
        focustime = 0.;
      }
      
      /* Count time in tenths of seconds to get the focus change */
      /* This is to give more feedback to the observer but */
      /* UTNow does not pick up fractions of a second so least count is 10 */
      
      focustime = focustime*36000.;

      if ( focusflag == -1 )
      {
        focuscount = focuscount - focusspd * ( (int) focustime);
      }
      if ( focusflag == 1 )
      {
        focuscount = focuscount + focusspd * ( (int) focustime);
      }
      focusflag = 0;
    }
    if ( focusflag == 0 )
    {
      
      /* Focus motion is already stopped so there is nothing to do */
    
    }  
  }    
}


/* Report the current focus count */
/* Use the readout from the encoder if it is available */
/* Otherwise use the timed focuscount saved in Focus() */

void GetFocus(double *telfocus)
{

  /* Focus motor device ID 0x12 */
  /* MC_GET_POSITION is 0x01 */
  /* Bytes in response are 0x03 */
  
  char focusCmd[] = { 0x50, 0x01, 0x12, 0x01, 0x00, 0x00, 0x00, 0x03 };
  char inputStr[2048];  
  int b0,b1,b2;
    
  /* Send the command */
  writen(TelPortFD,focusCmd,8);    
  
  /* Read a response */
  readn(TelPortFD,inputStr,4,1);
  
  /* Mask the high order bytes */
  b0 = (0x000EF & inputStr[2]);
  b1 = (0x000EF & inputStr[1]);
  b2 = (0x000EF & inputStr[0]);
    
  /* count = 256*256*b2 + 256*b1 + b0 */
  
  /* On our CDK20 focuser one count in b2 is approximately 9 microns */
  /* Therefore one count corresponds to 1 pixel at f/1 */
  /* At f/6.8 on the CDK20 it takes about 6.8 b2 counts to defocus 1 pixel */
  /* Only b2 is useful */
    
  /* Focuser diagnostic -- comment out except for testing */
  
  /* printf("Focus encoder b2=%d  b1=%d  b0=%d \n", b2,b1,b0); */ 
  /* printf ("Focus timer count=%d\n", focuscount); */
  
    
  *telfocus = (double) b2;
  
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
}


/* Time synchronization utilities */

/* Reset the telescope sidereal time */

int  SyncLST(double newTime)
{		
  fprintf(stderr,"NexStar does not support remote setting of sidereal time.\n");
  return -1;
}


/*  Reset the telescope local time */

int  SyncLocalTime()
{
  fprintf(stderr,"NexStar does not support remote setting of local time.\n");
  return -1;
}


/* Write serial port utility */

static int writen(fd, ptr, nbytes)
int fd;
void *ptr;
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


/* Read serial port utility */

static int readn(fd, ptr, nbytes, sec)
int fd;
void *ptr;
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


/* Examines the read status of a file descriptor.                  */
/* The timeout (sec, usec) specifies a maximum wait interval.      */
/* To effect a poll, the timeout (sec, usec) should be 0.          */
/* Returns >0 on data available.                                   */
/* Returns =0 if timeout expired.                                  */
/* Returns <0 on failure.                                          */
 
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

/* Set checksum from packet[start] through packet[stop] in packet[stop+1] */
/* Contents enumerated from packet[0]                                     */

void  checksum(unsigned char* packet, int start, int stop)
{
  unsigned char cksum;
  int len;
  len = stop - start + 1;
  packet = packet + start;
  cksum = 0;
  while (len--)
  {
    cksum+= *packet;
    packet++;
  }
  *packet = -cksum;
  return;
}
