/* --------------------------------------------------------------------------*/
/* -             Header for Celestron NexStar telescope control             -*/
/* --------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 2010 John Kielkopf                                              */
/* kielkopf@louisville.edu                                                   */
/*                                                                           */
/* Distributed under the terms of the General Public License (see LICENSE)   */
/*                                                                           */
/* Date: October 21, 2010                                                    */
/* Version: 5.2                                                              */
/*                                                                           */
/* History:                                                                  */
/*                                                                           */
/* September 8, 2006                                                         */
/*   Version 3.1                                                             */
/*                                                                           */
/* May 5, 2007                                                               */
/*   Version 4.0                                                             */
/*   Modified routines for compatibility with remote version of XmTel        */
/*                                                                           */
/* August 18, 2008                                                           */
/*   Version 4.1                                                             */
/*     Update for INDI version changes handling of pointing corrections      */
/*                                                                           */
/* September 7, 2008                                                         */
/*   Version 5.0 for INDI released                                           */
/*                                                                           */
/* December 30, 2008                                                         */
/*   Version 5.0.3 released                                                  */
/*     Definitions added to enable remote operation                          */
/*                                                                           */
/* November 29, 2009                                                         */
/*   Version 5.0.4 released                                                  */
/*     Focus calibration added                                               */
/*                                                                           */
/* August 11, 2010                                                           */
/*   Version 5.1   released                                                  */
/*     Removed timer from focus count                                        */
/*     Added device option for serial port                                   */
/*     Moved TELSERIAL TO configuration file                                 */
/*                                                                           */
/* October 23, 2010                                                          */
/*   Version 5.2                                                             */
/*   Focus routine made an external function                                 */



/* Focus and temperature files */

#define FOCUSFILE "/usr/local/observatory/status/focus"
#define TEMPERATUREFILE "/usr/local/observatory/status/temperature"
#define ROTATEFILE "/usr/local/observatory/status/rotate"
#define MAXPATHLEN 100

/* Set this for maximum slew rate allowed in degree/sec. */

#define MAXSLEWRATE	4 	/* 2 for safety; 4 for speed; 8 otherwise. */

/* Site parameters are no longer defined here.  Use the driver program header. */    

/* The difference between terrestrial and ephemeris time is determined */
/*   by the accumulated number of leapseconds at this moment.          */

/* Updated for January 1, 2009 from 33.0 */

#define LEAPSECONDS    34.0

/* Important physical constants */

#define PI             3.14159265358

/* The following parameters are used internally to set speed and direction. */
/* Do not change these values. */

/* Slew motion speeds */

#define	GUIDE		1
#define	CENTER		2
#define	FIND		4
#define	SLEW		8
 
/* Slew directions */

#define	NORTH		1
#define	SOUTH		2
#define	EAST		4
#define	WEST		8


/* Focus commands                             */
/* Distance from the CCD to the focal plane   */
/* Direction is + from the CCD toward the sky */
/* Used by Focus(focuscmd, focusspd)          */

#define FOCUSSPD4       4   /* Fast    */
#define FOCUSSPD3       3   /* Medium  */
#define FOCUSSPD2       2   /* Slow    */
#define FOCUSSPD1       1   /* Precise */

#define FOCUSCMDOUT     1   /* CCD moves away from sky */
#define FOCUSCMDOFF     0   /* CCD does not move */
#define FOCUSCMDIN     -1   /* CCD moves toward the  sky */


/* Rotator commands */

#define ROTATESPDFAST       3   /* Fast set */
#define ROTATESPDSLOW       2   /* Slow set */
#define ROTATESPDSIDEREAL   1   /* Sidereal rate */
#define ROTATECMDCW         1   /* Camera rotates CW looking toward the sky */
#define ROTATECMDOFF        0   /* Camera does not rotate */
#define ROTATECMDCCW       -1   /* Camera rotates CCW looking toward the sky */

/* Fan commands */

#define FANCMDHIGH      2   /* cooling fan on high speed */
#define FANCMDLOW       1   /* cooling fan on low speed */
#define FANCMDOFF       0   /* cooling fan off */

/* Dew heater commands */

#define HEATERCMDHIGH      2   /* dew heater on high */
#define HEATERCMDLOW       1   /* dew heater on low */
#define HEATERCMDOFF       0   /* dew heater off */

/* Pointing models (bit mapped and additive) */

#define RAW       0      /* Unnmodified but assumed zero corrected */
#define OFFSET    1      /* Correct for target offset*/
#define REFRACT   2      /* Correct for atmospheric refraction */
#define POLAR     4      /* Correct for polar axis misalignment */ 
#define DECAXIS   8      /* Correct for non-orthogonal angle between axes */
#define OPTAXIS  16      /* Correct for optical axis misalignment */
#define FLEXURE  32      /* Correct for flexure */

/* Display epochs (selected with GUI button) */

#define J2000    0      /* Epoch 2000.0 as entered or precessed without pm */
#define EOD      1      /* Epoch of date with pm if available  (default) */

/* Mounts */

#define ALTAZ  0        /* Alt-azimuth mount with at least 2-axis tracking */ 
#define EQFORK 1        /* Equatorial fork */
#define GEM    2        /* German equatorial */
