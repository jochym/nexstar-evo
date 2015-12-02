/* -------------------------------------------------------------------------- */
/* -                   Astronomical Telescope Control                       - */
/* -                         XmTel Header File                              - */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 2008-2014 John Kielkopf                                      */
/* kielkopf@louisville.edu                                                    */
/*                                                                            */
/* This file is part of XmTel.                                                */
/*                                                                            */
/* Date: April 4, 2014                                                        */
/* Version: 7.0                                                               */


/* Useful values */

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* User interface polling */

#define	POLLMS      1000   /* Poll period, ms */

/* Menu flags */

#define OK         1     
#define CANCEL     2

/* Menu item identifiers */

#define NEWQUEUE        1
#define NEWLOG          2
#define NEWCONFIG       3
#define EXIT            4
#define EDITQUEUE       5 
#define EDITLOG         6 
#define EDITCONFIG      7
#define POINTOPTION1    8
#define POINTOPTION2    9
#define POINTOPTION4   10
#define POINTOPTION8   11
#define REFTARGET      12
#define REFWCS         13
#define REFCLEAR       14
#define REFSAVE        15
#define REFRECALL      16
#define REFDEFAULT     17
#define MODELEDIT      18
#define MODELCLEAR     19
#define MODELSAVE      20
#define MODELRECALL    21
#define MODELDEFAULT   22

/* Target input flags */

#define RA        1               
#define DEC       2               

/* Configuration file */

#define CONFIGFILE "/usr/local/observatory/prefs/prefs.tel"

/* Reserve space for characters in file descriptors */

#define MAXPATHLEN  100

/* Default log and queue files */

#define LOGFILE   "telescope.log"
#define QUEUEFILE "target.que"


/* Default log and queue editor e.g. nedit or gedit */

#define XMTEL_EDITOR  "nedit"

/* Defaults for site may be modified by prefs file */

#define LONGITUDE      85.5300
#define LATITUDE       38.3334
#define ALTITUDE      230.0000
#define TEMPERATURE    20.0
#define PRESSURE      760.0
#define HOMEHA          -5.999
#define HOMEDEC         89.999
#define PARKHA          -5.999
#define PARKDEC         89.999
#define TELSERIAL     "/dev/ttyUSB0"
#define ARCSECPERPIX  0.54



/* Moore Observatory - Louisville, Kentucky USA */

/* #define LONGITUDE      85.5300 */
/* #define LATITUDE       38.3334 */
/* #define ALTITUDE      230.0000 */
/* #define HOMEHA          -5.999 */
/* #define HOMEDEC         89.999 */
/* #define PARKHA          -5.999 */
/* #define PARKDEC         89.999 */


/* Mt. Kent Observatgory - Toowoomba, Australia */

/*   #define LONGITUDE     -151.855278 */
/*   #define LATITUDE      -27.797778  */
/*   #define ALTITUDE      682.00      */



