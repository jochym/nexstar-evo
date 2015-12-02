/* ------------------------------------------------------------------------- */
/* -       Header for NexStar auxiliary command set telescope control       -*/
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Copyright 2010 John Kielkopf                                              */
/* kielkopf@louisville.edu                                                   */
/*                                                                           */
/* Distributed under the terms of the General Public License (see LICENSE)   */
/*                                                                           */
/* Date: October 21, 2010                                                    */
/* Version: 5.2.3                                                            */
/*                                                                           */
/* History:                                                                  */
/*                                                                           */
/* January 1, 2009                                                           */
/*   Version 1.0                                                             */
/*     Released                                                              */
/*                                                                           */
/* July 27, 2009                                                             */
/*   Version 1.1                                                             */
/*     Consistent with latest version                                        */
/*                                                                           */
/* November 28, 2009                                                         */
/*   Version 1.6                                                             */
/*     Default focus calibration added                                       */
/*                                                                           */
/* December 2, 2009                                                          */
/*   Version 1.8                                                             */
/*     Corrected cross meridian error                                        */
/*                                                                           */
/* August 21, 2010                                                           */
/*   Version 5.2.0                                                           */
/*   Updated to match latest protocol.c                                      */
/*   Added device option for serial port                                     */
/*   Bumped version number to match current xmtel                            */
/*   Added telserial to configuration file                                   */
/*                                                                           */
/* October 21, 2010                                                          */
/*   Version 5.2.3                                                           */
/*   Focus routine made an external function                                 */



/* Focus and temperature files */

#define FOCUSFILE "/usr/local/observatory/status/telfocus"
#define TEMPERATUREFILE "/usr/local/observatory/status/teltemperature"
#define ROTATEFILE "/usr/local/observatory/status/rotate"
#define MAXPATHLEN 100

/* CDK20 motors are 500 cpr x 5.9 gear x 3 cog x 360 worm = 3,186,000  cpr   */
/* Celestron processes this count and reports 24 bits per turn               */
/* Stepsize for RA and Dec encoders       */
/* 24 bits = 16777216                     */
/* Dec: 24 bits = 360 degrees             */
/* RA: 24 bits = 24 hours                 */

#define ALTCOUNTPERDEG 46603.3778      
#define AZCOUNTPERDEG  46603.3778

/* The protocol requires 24 bit counters                                     */
/* Search for 16777    in protocol.c to see where this is used               */
       
/* Set this for maximum slew rate in degree/sec for some controllers         */
/* A very safe choice is 2 but it takes a long time to slew at this rate     */
/* A compromise is 4 which is still slower than the CGE Pro default          */
/* A rate that is certainly too fast is the CGE Pro default of 8             */

/* Not only is a slower rate safer in case of a collision, but the           */
/* lower accelerations should reduce wear in robotic system with             */
/* frequent slews.                                                           */

/* Site parameters are not defined here.                                     */    

/* The difference between terrestrial and ephemeris time is determined       */
/*   by the accumulated number of leapseconds at this moment.                */

/* Updated on January 1, 2009 from 33.0                                      */

#define LEAPSECONDS    34.0

/* Important constant                                                        */

#define PI             3.14159265358

/* The following parameters are used internally to set speed and direction.  */
/* Do not change these values.                                               */

/* Slew motion speeds                                                        */

#define	GUIDE		1
#define	CENTER		2
#define	FIND		4
#define	SLEW		8
 
/* Slew directions                                                           */

#define	NORTH		1
#define	SOUTH		2
#define	EAST		4
#define	WEST		8


/* Focus commands                                                            */
/* Distance from the CCD to the focal plane                                  */
/* Direction is + from the CCD toward the sky                                */
/* Used by Focus(focuscmd, focusspd)                                         */

#define FOCUSSPD4       4   /* Fast    */
#define FOCUSSPD3       3   /* Medium  */
#define FOCUSSPD2       2   /* Slow    */
#define FOCUSSPD1       1   /* Precise */

#define FOCUSCMDOUT     1   /* CCD moves away from sky */
#define FOCUSCMDOFF     0   /* CCD does not move */
#define FOCUSCMDIN     -1   /* CCD moves toward the  sky */



/* Rotator commands                                                          */

#define ROTATESPDFAST       3   /* Fast set */
#define ROTATESPDSLOW       2   /* Slow set */
#define ROTATESPDSIDEREAL   1   /* Sidereal rate */
#define ROTATECMDCW         1   /* Camera rotates CW looking toward the sky */
#define ROTATECMDOFF        0   /* Camera does not rotate */
#define ROTATECMDCCW       -1   /* Camera rotates CCW looking toward the sky */



/* Fan commands                                                              */

#define FANCMDHIGH      2   /* Cooling fan on high speed */
#define FANCMDLOW       1   /* Cooling fan on low speed */
#define FANCMDOFF       0   /* Cooling fan off */

/* Dew heater commands                                                       */

#define HEATERCMDHIGH      2   /* dew heater on high */
#define HEATERCMDLOW       1   /* dew heater on low */
#define HEATERCMDOFF       0   /* dew heater off */

/* Pointing models (bit mapped and additive)                                 */

#define RAW       0      /* Unnmodified but assumed zero corrected */
#define OFFSET    1      /* Correct for target offset*/
#define REFRACT   2      /* Correct for atmospheric refraction */
#define POLAR     4      /* Correct for polar axis misalignment */ 
#define DECAXIS   8      /* Correct for non-orthogonal angle between axes */
#define OPTAXIS  16      /* Correct for optical axis misalignment */
#define FLEXURE  32      /* Correct for flexure */

/* Display epochs (selected with GUI button)                                 */

#define J2000    0      /* Epoch 2000.0 as entered or precessed without pm */
#define EOD      1      /* Epoch of date with pm if available  (default) */

/* Mounts                                                                    */

#define ALTAZ  0        /* Alt-azimuth mount with at least 2-axis tracking */ 
#define EQFORK 1        /* Equatorial fork */
#define GEM    2        /* German equatorial */

/* Slew tolerances intentionally kept large to minimize looping              */

#define SLEWTOLRA  0.006667  /* 0.1/15 hour    or 24 seconds of time */
#define SLEWTOLDEC 0.1       /* 0.1    degree  or  6 minutes of arc  */
#define SLEWFAST   0         /* Set to 1 for fast slew with caution !! */

/* Slew software limits                                                      */

#define MINTARGETALT   10.   /* Minimum target altitude in degrees */

