/* -----------------------------------------------------------                */
/* -         Protocol header for telescope control algoriths -                */
/* -----------------------------------------------------------                */
/*                                                                            */
/* Copyright 2008 John Kielkopf                                               */
/* kielkopf@louisville.edu                                                    */
/*                                                                            */
/* Distributed under the terms of the General Public License (see LICENSE)    */
/*                                                                            */
/* Date: July 2, 2012                                                         */
/* Version: 1.4                                                               */
/*                                                                            */
/* History:                                                                   */
/*                                                                            */
/* September 7, 2006                                                          */
/*   Version 1.0                                                              */
/*   Created algorithms header from nexstar protocol header                   */
/*                                                                            */
/* December 26, 2011                                                          */
/*   Version 1.3                                                              */
/*   Updated version of algorithms.c                                          */
/*                                                                            */
/* July 1, 2014                                                               */
/*   Version 1.4                                                              */
/*   Leapsecond increment                                                     */
/*                                                                            */


 
/* The difference between terrestrial and ephemeris time is determined */
/*   by the accumulated number of leapseconds at this moment.          */

#ifndef LEAPSECONDS
#define LEAPSECONDS    35.0
#endif

/* Important physical constants */

#ifndef PI
#define PI             3.14159265358
#endif
