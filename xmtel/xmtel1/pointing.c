/*                                                                            */
/* Pointing Corrections for Telescope Control                                 */
/*                                                                            */
/* Copyright 2008,2011,2012 John Kielkopf                                     */
/* kielkopf@louisville.edu                                                    */
/*                                                                            */
/* Distributed under the terms of the General Public License (see LICENSE)    */
/*                                                                            */
/* Date: March 7, 2012                                                        */
/* Version: 1.4                                                               */
/*                                                                            */
/* History:                                                                   */
/*                                                                            */
/*   March 26, 2006                                                           */
/*   Version 1.0                                                              */
/*                                                                            */
/*   June 16, 2007                                                            */
/*   Version 1.1                                                              */
/*   Included refraction routine here rather than in algorithms               */
/*   Modified structure to easily permit additional corrections               */
/*                                                                            */
/*   June 23, 2007                                                            */
/*   Version 1.2                                                              */
/*   Added correction for polar axis alignment                                */
/*   Requires external SiteLongitude and SiteLatitude                         */
/*   Requires external polaraz and polaralt                                   */
/*                                                                            */
/*   August 25, 2008                                                          */
/*   Version 1.3 in development                                               */
/*   Offset correction implemented here rather than in main program           */
/*   Requires external offsetha and offsetdec                                 */
/*                                                                            */
/*   September 12, 2008                                                       */
/*   Version 1.3 testing release                                              */
/*   Corrected equatorial-alt/az in algorithms package                        */
/*   Turned on for experimental testing in this released version:             */
/*     decaxis skew correction                                                */
/*     optical axis skew correction                                           */
/*     flexure correction                                                     */
/*   New: external parameter requirements                                     */
/*   Needed: procedure to define best parameters                              */
/*   Important: there will be errors here -- not all tested yet               */
/*                                                                            */
/*   September 27, 2008                                                       */
/*   Version 1.3                                                              */
/*   Offsets implemented in inditel and xmtel                                 */
/*                                                                            */
/*   October 9, 2008                                                          */
/*   Version 1.3.1                                                            */     
/*   Corrected treatment of negative declinations near the pole               */
/*                                                                            */
/*   December 17, 2008                                                        */
/*   Version 1.3.2                                                            */     
/*   PointingFromTel roundoff error near pole trapped                         */
/*   PointingFromTel refraction implementation revised                        */
/*                                                                            */
/*   December 28, 2011                                                        */
/*   Version 1.3.3                                                            */     
/*   Corrected error in refraction for large zenith angle                     */
/*                                                                            */
/*   February 27, 2012                                                        */
/*   Version 1.4                                                              */     
/*   Removed optical axis to be incorporated in dynamic empirical model               */
/*   Removed flexure axis to be incorporated in dynamic empirical model               */
/*   Added empirical dynamic model                                         */


/* References:                                                                */
/*                                                                            */
/* Explanatory Supplement to the Astronomical Almanac                         */
/*  P. Kenneth Seidelmann, Ed.                                                */
/*  University Science Books, Mill Valley, CA, 1992                           */
/*                                                                            */
/* Astronomical Formulae for Calculators, 2nd Edition                         */
/*  Jean Meeus                                                                */
/*  Willmann-Bell, Richmond, VA, 1982                                         */
/*                                                                            */
/* Astronomical Algorithms                                                    */
/*  Jean Meeus                                                                */
/*  Willmann-Bell, Richmond, VA, 1991                                         */
/*                                                                            */
/* Telescope Control                                                          */
/*  Mark Trueblood and Russell Merle Genet                                    */
/*  Willmann-Bell, Richmond, VA, 1997                                         */
/*                                                                            */

#include <stdio.h>
#include <math.h>
#include "protocol.h"

/* Prototypes */

void PointingFromTel (double *telra1, double *teldec1, 
  double telra0, double teldec0, int pmodel);
void PointingToTel (double *telra0, double *teldec0, 
  double telra1, double teldec1, int pmodel);
void Refraction(double *ha, double *dec, int dirflag);
void Polar(double *ha, double *dec, int dirflag);
void Decaxis(double *ha, double *dec, int dirflag);
void Model(double *ha, double *dec, int dirflag);

/* Calculation utilities */

extern double Map24(double hour);
extern double Map12(double hour);
extern double Map360(double angle);
extern double Map180(double angle);
extern double frac(double x);

/* Local sidereal time */

extern double LSTNow(void);

/* Coordinate transformations */

extern void EquatorialToHorizontal(double ha, double dec, double *az, double *alt);
extern void HorizontalToEquatorial(double az, double alt, double *ha, double *dec);

/* Telescope and observatory parameters                                          */

/* Coordinate offsets: telescope coordinates + offset = real value               */
/* If you point a telescope to ra,dec with offset turned on it will              */
/*   really be pointing at ra-offsetha, dec+offsetdec                            */
/*   Offset correction is the last applied going from raw to target coordinates  */
/*   It is the first applied going from target to raw pointing coordinates       */

extern double offsetha;      /* Hour angle (hours) */
extern double offsetdec;     /* Declination (degrees) */

/* Mounting polar axis correction: from true pole to mount polar axis direction */
/* Parameter polaralt will be + if the mount is above the true pole */
/* Parameter polaraz will be + if the mount is east of the true pole */

extern double polaralt;      /* Altitude (degrees) */
extern double polaraz;       /* Azimuth (degrees) */

/* Model corrections:  Real time behavior */
/* Internal units are pix/hr converted to angle as needed */

extern double modelha0,  modelha1;
extern double modeldec0, modeldec1;
extern double arcsecperpix;

/* Observatory coordinates */

extern double SiteLongitude;  
extern double SiteLatitude;   

/* Apply corrections to coordinates reported by the telescope              */
/* Input telescope raw coordinates assumed zero corrected                  */
/* Return real coordinates corresponding to this raw point                 */
/* Implemented in order: polar, decaxis, optaxis, flexure, refract, offset */
/* Note that the order is reversed from that in PointingFromTel            */

void PointingFromTel (double *telra1, double *teldec1, 
  double telra0, double teldec0, int pmodel)
{
  double tmpra, tmpha, tmpdec, tmplst;
  
  /* Find the LST for the pointing corrections and save it */
  /* Otherwise the lapsed time to do corrections affects the resultant HA and RA */
    
  tmplst = LSTNow();
  tmpra = telra0;
  tmpha = tmplst - tmpra;
  tmpha = Map12(tmpha);
  tmpdec = teldec0;
      
  /* Correct for mounting misalignment */
  /* For dir = -1 returns the real pointing given the apparent pointing */
  
  if ( pmodel == (pmodel | POLAR) )
  {
    Polar( &tmpha,  &tmpdec, -1);
  } 
  
  /* Correct for actual real time behavior  */
  /* For dir = -1 returns the real pointing given the apparent pointing */
  
  if ( pmodel == (pmodel | DYNAMIC) )
  {
    Model( &tmpha,  &tmpdec, -1);
  } 

  /* Correct for atmospheric refraction */
  /* For dir = -1 returns the real pointing given the apparent pointing */
    
  if ( pmodel == (pmodel | REFRACT) )
  {
    Refraction( &tmpha,  &tmpdec, -1);
  }

  /* Add offsets to apparent pointing to give real pointing */
  
  if ( pmodel == (pmodel | OFFSET) )
  {
    tmpha = tmpha + offsetha;
    tmpdec = tmpdec + offsetdec;
  }

  /* Now use celestial coordinates */
  
  tmpra = tmplst - tmpha;
    
  /* Handle a case that has crossed the 24 hr mark */
  
  tmpra = Map24(tmpra);
  
  /* Handle a case that has gone over the south pole */
  /* Limit set to 0.0001 to avoid inadvertent trip on roundoff error */
 
  if (tmpdec < -90.0002)
  {
     tmpdec = -180. - tmpdec;
     tmpra = tmpra + 12.;
     Map24(tmpra); 
  }
  
  /* Handle a case that has gone over the north pole */  
  /* Limit set to 0.0002 to avoid inadvertent trip on roundoff error */
  
  if (tmpdec > 90.0002)
  {
     tmpdec = 180. - tmpdec;
     tmpra = tmpra + 12.;
     Map24(tmpra); 
  } 
  
  *telra1 = tmpra;
  *teldec1 = tmpdec;

  return;
}  


/* Find the apparent coordinates to send to the telescope                  */
/* Input real target coordinates                                           */
/* Return apparent telescope coordinates to have it point at this target   */
/* Implemented in order: offset, refract, flexure, optaxis, decaxis, polar */
/* Note that the order is reversed from that in PointingFromTel            */

void PointingToTel (double *telra0, double *teldec0, 
  double telra1, double teldec1, int pmodel)
{
  double tmpra, tmpha, tmpdec, tmplst;
  
  tmpra = telra1;
  tmpdec = teldec1;


  /* Save the LST for later use, find the HA for this LST and finish the model */

  tmplst = LSTNow();
  tmpha = tmplst - tmpra;
  tmpha = Map12(tmpha); 


  /* Correct for offset */
  /* Offsets are defined as added to apparent pointing to give real pointing */
  
  if ( pmodel == (pmodel | OFFSET) )
  {
    tmpha = tmpha - offsetha;
    tmpdec = tmpdec - offsetdec;
  }  

   
  /* Correct for atmospheric refraction */
  /* For dir = +1 returns the apparent pointing given the real target pointing */
 
  if ( pmodel == (pmodel | REFRACT) )
  {
    Refraction( &tmpha,  &tmpdec, 1);
  }

  /* Correct for real time behavior */
  /* For dir = +1 returns the apparent pointing given the real pointing */
   
  if ( pmodel == (pmodel | DYNAMIC) )
  {
    Model( &tmpha,  &tmpdec, 1);
  } 

  /* Correct for mounting misalignment */
  /* For dir = +1 returns the apparent pointing given the real pointing */
  
  if ( pmodel == (pmodel | POLAR) )
  {
    Polar( &tmpha,  &tmpdec, 1);
  } 
  
  tmpra = tmplst - tmpha;

  /* Handle a case that has crossed the 24 hr mark */
  
  tmpra = Map24(tmpra);
  
  /* Handle a case that has gone over the south pole */
  
  if (tmpdec < -90.)
  {
     tmpdec = -180. - tmpdec;
     tmpra = tmpra + 12.;
     Map24(tmpra); 
  }
  
  /* Handle a case that has gone over the north pole */  
  
  if (tmpdec > 90.)
  {
     tmpdec = 180. - tmpdec;
     tmpra = tmpra + 12.;
     Map24(tmpra); 
  } 
       
  *telra0 = tmpra;
  *teldec0 = tmpdec;

  return;
}  


/* Correct ha and dec for atmospheric refraction                       */
/*                                                                     */
/* Call this in the form Refraction(&ha,&dec,dirflag)                  */
/*                                                                     */
/* Input:                                                              */
/*   Pointers to ha and dec                                            */
/*   Integer dirflag >=0 for add (real to apparent)                    */
/*   Integer dirflag <0  for subtract (apparent to real)               */
/*   Valid above 15 degrees.  Below 15 degrees uses 15 degree value.   */
/*   Global local barometric SitePressure (Torr)                       */
/*   Global local air temperature SiteTemperature (C)                  */
/* Output:                                                             */
/*   In place pointers to modified ha and dec                          */

void Refraction(double *ha, double *dec, int dirflag)
{
  double altitude, azimuth, dalt, arg;
  double hourangle, declination;
  extern double SiteTemperature;
  extern double SitePressure;

  /* Calculate the change in apparent altitude due to refraction */
  /* Uses 15 degree value for altitudes below 15 degrees */

  /* Real to apparent */
  /* Object appears to be higher due to refraction */
  
  hourangle = *ha;
  declination = *dec;
  EquatorialToHorizontal(hourangle, declination, &azimuth, &altitude);

  if (dirflag >= 0)
  {  
    if (altitude >= 15.)
    {
      arg = (90.0 - altitude)*PI/180.0;
      dalt = tan(arg);
      dalt = 58.276 * dalt - 0.0824 * dalt * dalt * dalt;
      dalt = dalt / 3600.;
      dalt = dalt * (SitePressure/(760.*1.01))*(283./(273.+SiteTemperature));
    }
   else
    {
      arg = (90.0 - 15.0)*PI/180.0;
      dalt = tan(arg);
      dalt = 58.276 * dalt - 0.0824 * dalt * dalt * dalt;
      dalt = dalt / 3600.;
      dalt = dalt * (SitePressure/(760.*1.01))*(283./(273.+SiteTemperature));
    }
    altitude = altitude + dalt;
  }
  
  /* Apparent to real */
  /* Object appears to be higher due to refraction */
  
  else if (dirflag < 0)
  {
    if (altitude >= 15.)
    {
      arg = (90.0 - altitude)*PI/180.0;
      dalt = tan(arg);
      dalt = 58.294 * dalt - 0.0668 * dalt * dalt * dalt;
      dalt = dalt / 3600.;
      dalt = dalt * (SitePressure/(760.*1.01))*(283./(273.+SiteTemperature));
    }
   else
    {
      arg = (90.0 - 15.0)*PI/180.0;
      dalt = tan(arg);
      dalt = 58.294 * dalt - 0.0668 * dalt * dalt * dalt;
      dalt = dalt / 3600.;
      dalt = dalt * (SitePressure/(760.*1.01))*(283./(273.+SiteTemperature));
    }

    altitude = altitude - dalt;
  }
   HorizontalToEquatorial(azimuth, altitude, &hourangle, &declination);
  *ha = hourangle;
  *dec = declination; 
}

/* Allow for known polar alignment parameters           */
/*                                                      */
/* Call this in the form Polar(&ha,&dec,dirflag)        */
/*                                                      */
/* Input:                                               */
/*   Pointers to ha and dec                             */
/*   Integer dirflag >=0  real to apparent              */
/*   Integer dirflag <0   apparent to real              */
/* Output:                                              */
/*   In place pointers to modified ha and dec           */
/* Requires:                                            */
/*   SiteLatitude in degrees                            */
/*   polaralt polar axis altitude error in degrees      */
/* polaraz  polar axis azimuth  error in degrees        */


void Polar(double *ha, double *dec, int dirflag)
{
  double hourangle, declination;
  double da, db;
  double epsha, epsdec;
  double latitude;
  
  /* Make local copies of input */
  
  hourangle = *ha;
  declination = *dec;  
  da = polaraz;
  db = polaralt;
  latitude = SiteLatitude;
  
  /* Convert to radians for the calculations */
   
  hourangle = hourangle*PI/12.;
  declination = declination*PI/180.;
  da = da*PI/180.;
  db = db*PI/180.; 
  latitude = latitude*PI/180.; 
  
  /* Evaluate the corrections in radians for this ha and dec */
  /* Sign check:  on the meridian at the equator */
  /*   epsha goes as -da */
  /*   epsdec goes as +db */
  /*   polaraz and polaralt signed + from the true pole to the mount pole */
    
  epsha = db*tan(declination)*sin(hourangle) - 
    da*(sin(latitude) - tan(declination)*cos(hourangle)*cos(latitude));
  epsdec = db*cos(hourangle) - da*sin(hourangle)*cos(latitude);    
    
  /* Real to apparent */
  
  if (dirflag >= 0)
  {  
    hourangle = hourangle + epsha;
    declination = declination + epsdec;
  }
  
  /* Apparent to real */
  
  else if (dirflag < 0)
  {
    hourangle = hourangle - epsha;
    declination = declination - epsdec;
  }

  /* Convert from radians to hours for ha and degrees for declination */

  hourangle = hourangle*12./PI;
  declination = declination*180./PI;
  
  /* Check for range and adjust to -90 -> +90 and 0 -> 24  */
 
  if (declination > 90. ) 
  {
    declination = 180. - declination;
    hourangle = hourangle + 12.;
  }
   if (declination < -90. ) 
  {
    declination = -180. - declination;
    hourangle = hourangle + 12.;
  }  

  hourangle = Map24(hourangle);         
  *ha = hourangle;
  *dec = declination;
}


/* Model corrections                                         */
/*                                                           */
/* Call this in the form Model(&ha,&dec,dirflag)             */
/*                                                           */
/* Input:                                                    */
/*   Pointers to ha and dec                                  */
/*   Integer dirflag >=0  real to apparent                   */
/*   Integer dirflag <0   apparent to real                   */
/* Output:                                                   */
/*   In place pointers to modified ha and dec                */
/* Requires global values:                                   */
/*   Model ha parameters (pixels/hour)                       */
/*   Model dec parameters (pixels/hour)                      */
/*   Scale arcsecperpix                                      */
/*                                                           */
/* Deltas add to apparent pointing to give real pointing     */
/* Values of parameters are global                           */

void Model(double *ha, double *dec, int dirflag)
{
  double tmpha, tmpdec;
  double dha, ddec;
  double deltaha;
  
  /* Make local copies of input */
  
  tmpha = *ha;
  tmpdec = *dec;
  
  deltaha = Map12(tmpha - modelha0);
  
  /* Calculate model dec increment in degrees from the reference point */
  
  ddec = deltaha*modeldec1*arcsecperpix/3600.;
  
  /* Calculate model ha increment in hours from the reference point */
  
  dha = deltaha*modelha1*arcsecperpix/54000.;
  
  if (dirflag >= 0)
  {  
    /* For dir = +1 returns the apparent pointing given the real pointing */
    
    tmpha = tmpha - dha;
    tmpdec = tmpdec - ddec;    
  }
  else if (dirflag < 0)
  {
    /* For dir = -1 returns the real pointing given the apparent pointing */

    tmpha = tmpha + dha;
    tmpdec = tmpdec + ddec;    
  }

  *ha = tmpha;
  *dec = tmpdec;
}

