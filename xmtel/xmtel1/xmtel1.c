/* -------------------------------------------------------------------------- */
/* -                   Astronomical Telescope Control                       - */
/* -                       XmTel User Interface                             - */
/* -                                                                        - */
/* -                        Standalone Version                              - */
/* -                                                                        - */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 2008-2014 John Kielkopf                                      */
/* kielkopf@louisville.edu                                                    */
/*                                                                            */
/* This file is part of XmTel.                                                */
/*                                                                            */
/* Distributed under the terms of the MIT License (see LICENSE)               */ 
/*                                                                            */
/* Date: September 30, 2014                                                   */
/* Version: 7.0                                                               */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/FileSB.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/RowColumn.h> 
#include <Xm/CascadeB.h>
#include <Xm/MessageB.h>
#include <Xm/Separator.h>
#include <Xm/SelectioB.h>
#include <Xm/Text.h>
#include <Xm/List.h>

#include "protocol.h"
#include "xmtel1.h"

/* Motif GUI */ 

XtAppContext context;
XmStringCharSet char_set=XmSTRING_DEFAULT_CHARSET;

/* Widgets for the menu bar */

Widget menu_bar;
Widget file_menu;
Widget newqueue_item;
Widget newlog_item;
Widget newconfig_item;
Widget exit_item;
Widget edit_menu;
Widget editqueue_item;
Widget editlog_item;
Widget editconfig_item;
Widget pointing_menu;
Widget p_options_toggle_1;
Widget p_options_toggle_2;
Widget p_options_toggle_4;
Widget p_options_toggle_8;
Widget p_options_toggle_16;
Widget p_options_toggle_32;
Widget ref_menu;
Widget ref_target_item;
Widget ref_wcs_item;
Widget ref_clear_item;
Widget ref_save_item;
Widget ref_recall_item;
Widget ref_default_item;
Widget model_menu;
Widget model_edit_item;
Widget model_clear_item;
Widget model_save_item;
Widget model_recall_item;
Widget model_default_item;



/* Widgets for the panel */

Widget toplevel;
Widget dialog;
Widget button[3][3];
Widget command[12];
Widget toggle[4];
Widget form;
Widget sep_btns;
Widget sep_coords;
Widget sep_queue;
Widget sep_status;
Widget label_mark;
Widget label_ra;
Widget label_dec;
Widget label_epoch;
Widget mark_tel;
Widget mark_target;

Widget get_telra;
Widget get_teldec;
Widget get_telepoch;
Widget get_targetra;
Widget get_targetdec;
Widget get_targetepoch;

Widget select_logfile;
Widget read_queuefile;
Widget select_configfile;

Widget queue_area;
Widget message_area;
Widget radio_box;

/* User interface polling */

unsigned long poll_interval = POLLMS;      
void poll_interval_handler(XtPointer client_data_ptr, XtIntervalId *client_id);            
XtIntervalId poll_interval_id;
XtPointer poll_interval_data_ptr;

Widget make_menu_item();     /* adds an pushbutton item into the menu */
Widget make_menu_toggle();   /* adds a toggle item into the menu      */
Widget make_menu();          /* creates a menu on the menu bar        */

/* Widgets for the model_edit panel */

Widget model_edit;


/* Labels for the guide buttons top to bottom then left to right */

static char btn_name[][6] = {
    "NoOp", 
    "E",
    "NoOp",
    "N",
    "NoOp",
    "S",
    "NoOp",
    "W",
    "NoOp"
};

/* Simple labels for the command area top to bottom, left to right */

static char cmd_name[][15] = {
    "Slew",
    "Save",
    "Track",
    "Recall",
    "Guide",  
    "Meridian",  
    "Stop",
    "Park"
};

/* Labels for the guide speed settings */

static char spd_name[][8] = {
    "Slew",
    "Find",
    "Center",
    "Guide"   
};



/* Message area strings */

char message[300];
static char message2[300];


/* Menu functions */

void create_menus(Widget menu_bar);    /* Creates all menus for program       */
void menuCB();             /* Callback routine used for all menus             */
void setup_model_edit();   /* Create the unmanaged model edit panel           */
void model_editCB();       /* Callback function for model edit panel          */

void dialogCB();           /* Callback function for the dialog box            */
void buttonCB();           /* Callback function for the paddle buttons        */
void speedCB();            /* Callback function for the slew speed buttons    */
void teldataCB();          /* Callback function for the data buttons          */
void commandCB();          /* Callback function for the command buttons       */
void selectlogCB();        /* Callback function for selecting the log file    */
void readqueueCB();        /* Callback function for opening the queue file    */
void selectconfigCB();     /* Callback function for reading a new config file */
void selectqueueCB();      /* Callback function for selecting a queue entry   */

/* Telescope functions */

void show_telescope_coordinates();      /* Display latest telescope coordinates */
void show_target_coordinates();         /* Display latest target coordinates  */
void show_guide_status();               /* Display the guide status */
void slew_telescope();                  /* Slew telescope to target */
void check_slew_status();               /* Monitor slew progress */
void fetch_telescope_coordinates();     /* Import current telescope coordinates */

/* Reference management management */

void target_telescope_reference(void);  /* Set offset reference to target coordinates */
void clear_telescope_reference(void);   /* Set offsets to zero */
void recall_telescope_reference(void);  /* Recall saved reference offsets from the status directory */
void save_telescope_reference(void);    /* Save offset reference in the status directory */
void default_telescope_reference(void); /* Set offsets to defaults */
void wcs_telescope_reference(void);     /* Use a WCS header reference from the status directory */

/* Model management */

void clear_telescope_model(void);   /* Set model parameters to zero */
void recall_telescope_model(void);  /* Recall saved model from the status directory */
void save_telescope_model(void);    /* Save model in the status directory */
void default_telescope_model(void); /* Use the default model */

/* Message area display */

void show_message();                  /* Update message area with string message */

/* Local log and queue operations */

void save_coordinates();                /* Save a log entry and add to the history */
void recall_coordinates();              /* Read the next previous history entry */
void read_queue();                      /* Read queue file into memory */

/* User interface RA and Dec direct entry */

void new_target_ra();                   /* User input of target RA */
void new_target_dec();                  /* User input of target Dec */

/* Interface to local sky display */

void link_fifos();                      /* Startup fifo link routine */
void unlink_fifos();                    /* Shutdown fifo link routine */
void mark_xephem_telescope();           /* Export telescope coordinates to XEphem */
void read_xephem_target();              /* Read target coordinates from XEphem goto fifo */
void mark_xephem_target();              /* Export target coordinates to XEphem */

/* Startup and shutdown */

void link_telescope();                  /* Startup telescope link routine */
void unlink_telescope();                /* Shutdown telescope link routine */


/* -------------------------------------------------------- */
/*       Telescope and mounting commands                    */
/*       External routines defined in the protocols         */
/* -------------------------------------------------------- */

/* Interface control */

extern void ConnectTel(void);         
extern void DisconnectTel(void);
extern int  CheckConnectTel(void);

/* Slew and track control */

extern void SetRate(int newRate);
extern void StartSlew(int direction);
extern void StopSlew(int direction);
extern void StartTrack(void);
extern void CenterGuide(double centerra, double centerdec, 
  int raflag, int decflag, int pmodel);
extern void StopTrack(void);
extern void FullStop(void);

/* Celestial coordinate read, write, and go to */

extern void GetTel(double *telra, double *teldec, int pmodel);
extern int  GoToCoords(double newRA, double newDec, int pmodel);
extern int  CheckGoTo(double desRA, double desDec, int pmodel);


/* Corrections for proper motion, precession, aberration, and nutation */

extern void Apparent(double *ra, double *dec, int dirflag);  
extern void PrecessToEOD(double epoch, double  *ra, double  *dec);
extern void PrecessToEpoch(double epoch, double  *ra, double  *dec);
extern void ProperMotion(double epoch, double *ra, double *dec, 
  double pm_ra, double pm_dec); 
extern void TestAlgorithms(void);

/* Time from the computer system processed by the algorithms package */

extern double LSTNow(void);
extern double UTNow(void);

/* Utility */

extern double Map12(double ha);
extern double Map24(double ra);
extern double Map180(double dec);


  
/* ----------------------------------- */
/* ---- End of external routines  ---- */
/* ----------------------------------- */

/* Convert string deg:min:sec to double */

int dmstod (char *instr, double *datap);

/* Convert dms double to string deg:min:sec */

void dtodms (char *outstr, double *dmsp); 

/* Telescope control variables */

double telha, telra, teldec;   /* telescope ha, ra and dec at eod */
double targetra, targetdec;    /* target ra and dec at eod */


double offsetha = 0.;
double offsetdec = 0.;
double offsetha_default = 0.;
double offsetdec_default = 0.;
double polaraz = 0.;
double polaralt = 0.;
double arcsecperpix = ARCSECPERPIX;
double modelha0 = 0.;
double modelha1 = 0.;
double modeldec0 = 0.;
double modeldec1 = 0.;
double modelha1_default = 0.;
double modeldec1_default = 0.;


/* User interface flags and variables  */

int quiet = TRUE;                  /* set FALSE for diagnostics */
int display_telepoch=EOD;          /* display epoch for telescope */
int display_targetepoch=EOD;       /* display epoch for target  */
int pmodel=RAW;                    /* pointing model default to raw data */
int telspd;                        /* drive speed */
int teldir;                        /* drive direction */                        
int telflag=FALSE;                 /* telescope connection flag */
int target;                        /* flag for target input */
int gotoflag = FALSE;              /* goto progress flag */

/* Center guide control */

int guidedecflag=0;                /* Flag to control dec guide function */
int guideraflag=0;                 /* Flag to control ra  guide function */
int guideflag=0;                   /* Flag to control center guide */
double guidera;                    /* Guiding center ra and dec */ 
double guidedec;                   /* Guiding center ra and dec */ 
  
/* Site */

double SiteLatitude = LATITUDE;        /* Latitude in degrees + north */  
double SiteLongitude = LONGITUDE;      /* Longitude in degrees + west */  
double SiteAltitude = ALTITUDE;        /* Altitude in meters */
double SitePressure = PRESSURE;        /* Atmospheric pressure in Torr */
double SiteTemperature = TEMPERATURE ; /* Local atmospheric temperature in C */

/* Mount */

int  homenow = FALSE;                  /* Flag TRUE queries "now" at startup */
int  telmount = GEM;                   /* One of GEM, EQFORK, ALTAZ          */
double nowut, nowlst;                  /* Used for global current times      */
double homeha = HOMEHA;                /* Startup HA                         */
double homedec = HOMEDEC;              /* Startup Dec                        */
double homera = HOMEHA;                /* RA at instant of startup           */
double parkha = PARKHA;                /* Park telescope at this HA          */
double parkdec = PARKDEC;              /* Park telescope at this Dec         */
char   telserial[32];                  /* Serial port if needed              */


/* Files */

int fd_fifo_in, fd_fifo_out;           /* FIFO file descriptors */
FILE *fp_log;                          /* Log file pointer      */
static char *logfile;                  /* Log name */
FILE *fp_queue;                        /* Queue file pointer */
static char *queuefile;                /* Queue name */
FILE *fp_config;                       /* Configuration file pointer */
static char *configfile;               /* Configuration name */
void read_config(void);                /* Configuration file reader */
void write_coords(double ra, double dec);  /* Write ra and dec to tmp */

/* External commands */

static char *editor;                   /* external queue and log editor */

/* Buffers */

char buf[256];                         /* general purpose character buffer */

/* Queue */

int nqueue = 0;                        /* number of entries in the queue */
int queuechoice = 0;                   /* serial number of selected entry */

/* History */

/* The history catalog is a ring buffer starting at 0 for the first entry. */
/* There are nhistory entries in use (<=100). */

int nhistory = 0;                      /* number of history entries (max 100) */
int last_history_saved = -1;           /* location for last history saved */
int last_history_recalled = -1;        /* location for last history recalled */


typedef struct                         /* catalog structure assuming EOD */
{
  char name[80];
  char desc[80];
  double ra;
  double dec;
  double mag;
} catalog;

/* Create the space for catalogs.  Must keep track not to overrun. */
/* Allow one extra entry and watch for maximum number in operations. */
/* History is indexed from 1 for first entry, so 0th entry is not used. */

catalog queue[10001];                  /* This holds the observing queue */
catalog history[101];                  /* This holds the saved coordinates */

/* Flags */

char option_state;

/* End of global definitions */


int main(argc,argv)
  int argc; 
  char *argv[];
{
  Arg al[10];
  int ac;
  unsigned long x,y,btn_number,cmd_number;
  EventMask mask;

  fprintf(stdout,"Starting xmtel \n");

  /* Set the default log file */
  
  logfile = (char *) malloc (MAXPATHLEN);
  strcpy(logfile,LOGFILE);
  
  fprintf(stdout,"Log file selected \n");

  
  /* Set the default queue file */
  
  queuefile = (char *) malloc (MAXPATHLEN);
  strcpy(queuefile,QUEUEFILE);
  
  fprintf(stdout,"Queue file selected \n");
  
  /* Set the default configuration file */

  configfile = (char *) malloc (MAXPATHLEN);
  strcpy(configfile,CONFIGFILE);
  
  fprintf(stdout,"Configure file selected \n");

  /* Set the default editor */

  editor = (char *) malloc (MAXPATHLEN);
  strcpy(editor,XMTEL_EDITOR);

  fprintf(stdout,"File editor selected \n"); 

  /* Set the default serial port */
  
  strcpy (telserial,TELSERIAL);
    
  fprintf(stdout,"Serial port selected \n");
      
  /* Read the configuration file and modify defaults as needed */
  
  read_config();
  
  fprintf(stdout, "Configuration file read \n");


  /* Start the fifo communications first */
  /* Start XEphem after XmTel is running */
  
  link_fifos();
  
  fprintf(stdout, "FIFO communications started \n");  
  
  /* Start the Motif GUI */
 
  /* Create the toplevel shell */
  toplevel = XtAppInitialize(&context,"",NULL,0,&argc,argv,
      NULL,NULL,0);

  /* Set the default size of the window */ 
  ac=0;
  XtSetArg(al[ac],XmNwidth,400); ac++;
  XtSetArg(al[ac],XmNheight,700); ac++;
  XtSetValues(toplevel,al,ac);

  /* Create a form widget */
  ac=0;
  form=XmCreateForm(toplevel,"form",al,ac);
  XtManageChild(form);

  /* Create the menu bar and attach it to the form */
  ac=0;
  XtSetArg(al[ac],XmNtopAttachment,XmATTACH_FORM); ac++;
  XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++;
  XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;
  menu_bar=XmCreateMenuBar(form,"menu_bar",al,ac);
  XtManageChild(menu_bar);

  /* Note that locations of buttons are in percent of window dimension */

  /* Create a radio box to hold the speed toggles and attach it to form */
  
  ac=0;
  XtSetArg(al[ac],XmNorientation,XmVERTICAL); ac++;
  XtSetArg(al[ac],XmNtopAttachment,XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,12); ac++; 
  XtSetArg(al[ac],XmNleftAttachment,XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,5); ac++; 
  XtSetArg(al[ac],XmNrightAttachment,XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,24); ac++; 
  radio_box=XmCreateRadioBox(form,"radio_box",al,ac);
  XtManageChild(radio_box);
  
  /* Create speed toggles */
  
  for (x=0; x<4; x++)
  {
    ac=0;
    XtSetArg(al[ac],XmNlabelString,
      XmStringCreate(spd_name[x],char_set)); ac++;
    toggle[x]=XmCreateToggleButton(radio_box,"toggle",al,ac);
    XtManageChild(toggle[x]);
    XtAddCallback (toggle[x], XmNvalueChangedCallback, speedCB, (XtPointer) x);
  }     
 
  /* Set the slew speed togglebutton to the startup state */

  XmToggleButtonSetState (toggle[1], True, False);
  

  /* Set up the slew buttons, add event handlers, attach to form. */

  for (x=0; x<3; x++)
  {
    for (y=0; y<3; y++)
    {
      btn_number=3*x+y;
      if ( (btn_number==1) | (btn_number==3) | (btn_number==5) | (btn_number==7) ) 
      {
        ac=0;
        XtSetArg(al[ac],XmNlabelString,
            XmStringCreate(btn_name[btn_number],char_set)); ac++;
        XtSetArg(al[ac],XmNleftAttachment,
            XmATTACH_POSITION); ac++;
        XtSetArg(al[ac],XmNleftPosition,28+x*15); ac++;
        XtSetArg(al[ac],XmNrightAttachment,
            XmATTACH_POSITION); ac++;
        XtSetArg(al[ac],XmNrightPosition,42+x*15); ac++;
        XtSetArg(al[ac],XmNtopAttachment,
            XmATTACH_POSITION); ac++;
        XtSetArg(al[ac],XmNtopPosition,6+y*9); ac++;
        XtSetArg(al[ac],XmNbottomAttachment,
            XmATTACH_POSITION); ac++;
        XtSetArg(al[ac],XmNbottomPosition, 15+y*9); ac++;
        button[x][y]=XmCreatePushButton(form,"label",al,ac);
        mask = ButtonReleaseMask | ButtonPressMask;
        XtAddEventHandler(button[x][y],mask,False,
            buttonCB,(XtPointer) btn_number);              
        XtManageChild(button[x][y]);
      }
    }
  }

  /* Create a separator widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNtopAttachment,XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,35); ac++;
  XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++; 
  XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;  
  XtSetArg(al[ac],XmNbottomAttachment,XmATTACH_NONE); ac++;  
  sep_btns=XmCreateSeparator(form,"sep",al,ac);
  XtManageChild(sep_btns);   
 

  /* Build the telescope and target coordinates display area */
  
  /* Create the mark label widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("Mark",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,13); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,31); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,36); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,40); ac++;
  label_mark=XmCreateLabel(form,"label",al,ac);
  XtManageChild(label_mark); 
  

  /* Create the ra label widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("RA",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,37); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,45); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,36); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,40); ac++;
  label_ra=XmCreateLabel(form,"label",al,ac);
  XtManageChild(label_ra); 

  /* Create the dec label widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("Dec",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,55); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,63); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,36); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,40); ac++;
  label_dec=XmCreateLabel(form,"label",al,ac);
  XtManageChild(label_dec);    

  /* Create the epoch label widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("Epoch",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,72); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,80); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,36); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,40); ac++;
  label_epoch=XmCreateLabel(form,"label",al,ac);
  XtManageChild(label_epoch);    


  /* Create the telescope label widget and attach it to the form */

  cmd_number = 0;
  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("Telescope",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,13); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,31); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,40); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,44); ac++;
  mark_tel=XmCreatePushButton(form,"label",al,ac);
  XtManageChild(mark_tel);
  XtAddCallback(mark_tel,XmNactivateCallback,
                teldataCB,(XtPointer) cmd_number);     
       

  /* Create the target label widget and attach it to the form */

  cmd_number = 1;
  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("Target",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,13); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,31); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,44); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,48); ac++;
  mark_target=XmCreatePushButton(form,"label",al,ac);
  XtManageChild(mark_target);    
  XtAddCallback(mark_target,XmNactivateCallback,
                teldataCB,(XtPointer) cmd_number);     
 


  /* Create a telescope ra widget and attach it to the form */

  cmd_number = 2;    
  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("00:00:00",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,33); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,49); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,40); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,44); ac++;
  get_telra=XmCreatePushButton(form,"label",al,ac);
  XtManageChild(get_telra);
  XtAddCallback(get_telra,XmNactivateCallback,
                teldataCB,(XtPointer) cmd_number);     
 

  /* Create a telescope dec widget and attach it to the form */

  cmd_number = 3;    
  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("00:00:00",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,51); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,67); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,40); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,44); ac++;
  get_teldec=XmCreatePushButton(form,"label",al,ac);
  XtManageChild(get_teldec);
  XtAddCallback(get_teldec,XmNactivateCallback,
                teldataCB,(XtPointer) cmd_number);    


  /* Create a telescope epoch widget and attach it to the form */

  cmd_number = 4;  
  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("Now",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,69); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,83); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,40); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,44); ac++;
  get_telepoch=XmCreatePushButton(form,"label",al,ac);
  XtManageChild(get_telepoch);
  XtAddCallback(get_telepoch,XmNactivateCallback,
                teldataCB,(XtPointer) cmd_number);     
  
  
  /* Create a target ra widget and attach it to the form */

  cmd_number = 5;  
  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("00:00:00",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,33); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,49); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,44); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,48); ac++;
  get_targetra=XmCreatePushButton(form,"label",al,ac);
  XtManageChild(get_targetra);
  XtAddCallback(get_targetra,XmNactivateCallback,
                teldataCB,(XtPointer) cmd_number);
                        

  /* Create a target dec widget and attach it to the form */

  cmd_number = 6;  
  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("00:00:00",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,51); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,67); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,44); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,48); ac++;
  get_targetdec=XmCreatePushButton(form,"label",al,ac);
  XtManageChild(get_targetdec);
  XtAddCallback(get_targetdec,XmNactivateCallback,
                teldataCB,(XtPointer) cmd_number);    

  
  /* Create a target epoch widget and attach it to the form */

  cmd_number = 7;  
  ac=0;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreate("Now",char_set)); ac++;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,69); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,83); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,44); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,48); ac++;
  get_targetepoch=XmCreatePushButton(form,"label",al,ac);
  XtManageChild(get_targetepoch);
  XtAddCallback(get_targetepoch,XmNactivateCallback,
                teldataCB,(XtPointer) cmd_number);        
  

  /* Create a separator widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNtopAttachment,XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,50); ac++;
  XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++; 
  XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;  
  XtSetArg(al[ac],XmNbottomAttachment,XmATTACH_NONE); ac++;      
  sep_coords=XmCreateSeparator(form,"sep",al,ac);
  XtManageChild(sep_coords);  
  

  /* Queue area */ 
  
  /* Create a scrolled queue list widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,8); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,92); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,52); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,63); ac++;
  queue_area=XmCreateScrolledList(form,"queue_area",al,ac);
  XtManageChild(queue_area);
  XtAddCallback(queue_area,XmNdefaultActionCallback,selectqueueCB,NULL);
  XtAddCallback(queue_area,XmNbrowseSelectionCallback,selectqueueCB,NULL);

    
  /* Create a separator widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNtopAttachment,XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,65); ac++;
  XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++; 
  XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;  
  XtSetArg(al[ac],XmNbottomAttachment,XmATTACH_NONE); ac++;      
  sep_queue=XmCreateSeparator(form,"sep",al,ac);
  XtManageChild(sep_queue);   

  /* Set up the 8 command buttons and attach them to the form */

  for (x=0; x<4; x++)   
  for (y=0; y<2; y++)
  {
    cmd_number=2*x+y;
    ac=0;
    XtSetArg(al[ac],XmNlabelString,
      XmStringCreate(cmd_name[cmd_number],char_set)); ac++;
    XtSetArg(al[ac],XmNleftAttachment,
      XmATTACH_POSITION); ac++;
    XtSetArg(al[ac],XmNleftPosition,3+x*24); ac++;
    XtSetArg(al[ac],XmNrightAttachment,
      XmATTACH_POSITION); ac++;
    XtSetArg(al[ac],XmNrightPosition,25+x*24); ac++;
    XtSetArg(al[ac],XmNtopAttachment,
      XmATTACH_POSITION); ac++;
    XtSetArg(al[ac],XmNtopPosition,67+y*9); ac++;
    XtSetArg(al[ac],XmNbottomAttachment,
      XmATTACH_POSITION); ac++;
    XtSetArg(al[ac],XmNbottomPosition, 75+y*9); ac++;
    command[y]=XmCreatePushButton(form,"label",al,ac);
    XtManageChild(command[y]);
    XtAddCallback(command[y],XmNactivateCallback,
      commandCB,(XtPointer) cmd_number);
  }

  /* Create a separator widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNtopAttachment,XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,86); ac++;
  XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++; 
  XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;  
  XtSetArg(al[ac],XmNbottomAttachment,XmATTACH_NONE); ac++;      
  sep_status=XmCreateSeparator(form,"sep",al,ac);
  XtManageChild(sep_status);   


  /* Message area*/

  /* Create a message widget and attach it to the form */

  ac=0;
  XtSetArg(al[ac],XmNleftAttachment,
    XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNleftPosition,8); ac++;
  XtSetArg(al[ac],XmNrightAttachment,
    XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNrightPosition,92); ac++;
  XtSetArg(al[ac],XmNtopAttachment,
    XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNtopPosition,89); ac++;
  XtSetArg(al[ac],XmNbottomAttachment,
    XmATTACH_POSITION); ac++;
  XtSetArg(al[ac],XmNbottomPosition,98); ac++;
  XtSetArg(al[ac],XmNeditable,False); ac++;
  XtSetArg(al[ac],XmNeditMode,XmMULTI_LINE_EDIT); ac++;
  message_area=XmCreateScrolledText(form,"message_area",al,ac);
  XtManageChild(message_area);

  /* Create a prompt dialog for target input but leave it unmanaged */

  ac=0;
  XtSetArg(al[ac], XmNselectionLabelString,
    XmStringCreateLtoR("Enter coordinates: ",char_set)); ac++;
  XtSetArg(al[ac], XmNdialogTitle,
    XmStringCreateLtoR("XmTel: target",char_set)); ac++;
  dialog=XmCreatePromptDialog(toplevel,"dialog",al,ac);
  XtAddCallback(dialog, XmNokCallback, dialogCB, (XtPointer) OK);
  XtAddCallback(dialog, XmNcancelCallback, dialogCB, (XtPointer) CANCEL);
  XtUnmanageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));

  /* Create a dialog to select the log file but leave it unmanaged */

  ac=0;
  XtSetArg(al[ac],XmNmustMatch,True); ac++;
  XtSetArg(al[ac],XmNautoUnmanage,False); ac++;
  select_logfile=XmCreateFileSelectionDialog(toplevel,"select_logfile",al,ac);
  XtAddCallback (select_logfile, XmNokCallback, selectlogCB, (XtPointer) OK);
  XtAddCallback (select_logfile, XmNcancelCallback, selectlogCB, (XtPointer) CANCEL);
  XtUnmanageChild(XmSelectionBoxGetChild(select_logfile,
    XmDIALOG_HELP_BUTTON));
  
  /* Create a dialog to select and read the queue but leave it unmanaged */

  ac=0;
  XtSetArg(al[ac],XmNmustMatch,True); ac++;
  XtSetArg(al[ac],XmNautoUnmanage,False); ac++;
  read_queuefile=XmCreateFileSelectionDialog(toplevel,"read_queuefile",al,ac);
  XtAddCallback (read_queuefile, XmNokCallback, readqueueCB, (XtPointer) OK);
  XtAddCallback (read_queuefile, XmNcancelCallback, readqueueCB, (XtPointer) CANCEL);
  XtUnmanageChild(XmSelectionBoxGetChild(read_queuefile,
    XmDIALOG_HELP_BUTTON));

  /* Create a dialog to select and read the configuration but leave it unmanaged */

  ac=0;
  XtSetArg(al[ac],XmNmustMatch,True); ac++;
  XtSetArg(al[ac],XmNautoUnmanage,False); ac++;
  select_configfile=XmCreateFileSelectionDialog(toplevel,"select_configfile",al,ac);
  XtAddCallback (select_configfile, XmNokCallback, selectconfigCB, (XtPointer) OK);
  XtAddCallback (select_configfile, XmNcancelCallback, selectconfigCB, (XtPointer) CANCEL);
  XtUnmanageChild(XmSelectionBoxGetChild(select_configfile,
    XmDIALOG_HELP_BUTTON));

  /* Create the menubar */
  
  create_menus(menu_bar);  
  
  /* Connect to XEphem's goto fifo */

  if (fd_fifo_in > 0)
      XtAppAddInput (context, fd_fifo_in, (XtPointer)XtInputReadMask,
        read_xephem_target, NULL);          

  /* Create unmanaged control panels */

  setup_model_edit();
  
  
  /* Service status display and slewing poll */

  poll_interval_id = XtAppAddTimeOut(context,
    poll_interval,poll_interval_handler,poll_interval_data_ptr);  

  /* Start the user interface and telescope communications */
      
  XtRealizeWidget(toplevel);
  
  link_telescope();

  XtAppMainLoop(context);

  exit(EXIT_SUCCESS);
}

/* Add an item into a menu in the menubar */  

Widget make_menu_item(char *item_name, unsigned long client_data, Widget menu)

{
  int ac;
  Arg al[16];
  Widget item;

  ac = 0;
  XtSetArg(al[ac], XmNlabelString,
      XmStringCreateLtoR(item_name,char_set)); ac++;      
  item=XmCreatePushButton(menu,item_name,al,ac);
  XtManageChild(item);
  XtAddCallback (item,XmNactivateCallback,menuCB,(XtPointer) client_data);      
  XtSetSensitive(item,True);
  return(item);
}

/* Add a toggle item into a menu in the menubar */     

Widget make_menu_toggle(char *item_name, unsigned long client_data, Widget menu)

{
  int ac;
  Arg al[16];
  Widget item;

  ac = 0;
  XtSetArg(al[ac], XmNlabelString,
      XmStringCreateLtoR(item_name,char_set)); ac++;      
  item=XmCreateToggleButton(menu,item_name,al,ac);
  XtManageChild(item);
  XtAddCallback (item,XmNvalueChangedCallback,menuCB,(XtPointer) client_data);      
  XtSetSensitive(item,True);
  return(item);
}

/* Creates a menu on the menu bar */

Widget make_menu(char *menu_name, Widget menu_bar)

{
  int ac;
  Arg al[16];
  Widget menu, cascade;

  menu=XmCreatePulldownMenu(menu_bar,menu_name,NULL,0);
  ac=0;
  XtSetArg(al[ac],XmNsubMenuId,menu);  ac++;
  XtSetArg(al[ac],XmNlabelString,
      XmStringCreateLtoR(menu_name,char_set)); ac++;
  cascade=XmCreateCascadeButton(menu_bar,menu_name,al,ac);      
  XtManageChild (cascade); 
  return(menu);
}

/* Create all the menubar menus */

void create_menus(Widget menu_bar)
{
  /* Create the file menu */
  file_menu=make_menu("File",menu_bar);
  newqueue_item=make_menu_item("New Queue",NEWQUEUE,file_menu); 
  newlog_item=make_menu_item("New Log",NEWLOG,file_menu); 
  newconfig_item=make_menu_item("New Config",NEWCONFIG,file_menu);
  exit_item=make_menu_item("Exit XmTel",EXIT,file_menu);

  /* Create the edit menu */
  edit_menu=make_menu("Edit",menu_bar);
  editqueue_item=make_menu_item("Queue",EDITQUEUE,edit_menu);
  editlog_item=make_menu_item("Log",EDITLOG,edit_menu);
  editconfig_item=make_menu_item("Configuration",EDITCONFIG,edit_menu);
  
  /* Create the pointing menu with toggle options */
  pointing_menu=make_menu("Pointing",menu_bar);
  p_options_toggle_1  = make_menu_toggle("Offset",POINTOPTION1,pointing_menu);
  p_options_toggle_2  = make_menu_toggle("Refraction",POINTOPTION2,pointing_menu);
  p_options_toggle_4  = make_menu_toggle("Polar",POINTOPTION4,pointing_menu);
  p_options_toggle_8  = make_menu_toggle("Model",POINTOPTION8,pointing_menu);

  /* Create the reference pull-down menu */
  ref_menu            = make_menu("Reference",menu_bar);
  ref_target_item     = make_menu_item("Target",REFTARGET,ref_menu);
  ref_wcs_item        = make_menu_item("WCS",REFWCS,ref_menu); 
  ref_clear_item      = make_menu_item("Clear",REFCLEAR,ref_menu);
  ref_save_item       = make_menu_item("Save",REFSAVE,ref_menu);
  ref_recall_item     = make_menu_item("Recall",REFRECALL,ref_menu);
  ref_default_item    = make_menu_item("Default",REFDEFAULT,ref_menu);
  
  /* Create the model pull-down menu */
  model_menu            = make_menu("Model",menu_bar);
  model_edit_item       = make_menu_item("Edit",MODELEDIT,model_menu);
  model_clear_item      = make_menu_item("Clear",MODELCLEAR,model_menu);
  model_save_item       = make_menu_item("Save",MODELSAVE,model_menu);
  model_recall_item     = make_menu_item("Recall",MODELRECALL,model_menu);
  model_default_item    = make_menu_item("Default",MODELDEFAULT,model_menu);
 
}

/* Callback routines used for menubar */

void menuCB(w,client_data,call_data)
  Widget w;
  int client_data;
  XmAnyCallbackStruct *call_data;
{

  /* if request for a queue file detected, then select a new queue file */

  if (client_data==NEWQUEUE)
  {
    /* make the queue selection dialog appear */
    
    XtManageChild(read_queuefile);    
  }
  
  /* if request for a new log file detected, then select a new log file */

  if (client_data==NEWLOG)
  {
    /* make the log file dialog appear */
    
    XtManageChild(select_logfile);
  } 

  /* if request for a new configuration file detected, then select and read a new config file */

  if (client_data==NEWCONFIG)
  {
    /* make the configuration file dialog appear */
    
    XtManageChild(select_configfile);
  }     
  
  /* if exit detected, then save last location and exit cleanly */
  
  if (client_data==EXIT) 
  {
    unlink_telescope();
    unlink_fifos();
    exit(EXIT_SUCCESS);
  }   

  /* if request to edit the queue is detected, then edit the queue file */

  if (client_data==EDITQUEUE)
  {
    strcpy(buf, editor);
    strcat(buf, " ");
    strcat(buf, queuefile);
    system(buf); 
  } 
  
  /* if request to edit log file detected, then edit the log file */

  if (client_data==EDITLOG)
  {
    strcpy(buf, editor);
    strcat(buf, " ");
    strcat(buf, logfile);
    system(buf); 
  }
  
  /* if request to edit config file detected, then edit the config file */

  if (client_data==EDITCONFIG)
  {
    strcpy(buf, editor);
    strcat(buf, " ");
    strcat(buf, configfile);
    system(buf); 
  }
              
  /* Detect a change to the toggle for a pointing option */
  /* Check for the state of the option */
  /* Set pmodel using bitwise OR or bitwise AND NOT */

  /* if a change in pointing option is detected, then set pmodel */

  if (client_data==POINTOPTION1)
  {
  
    if( XmToggleButtonGetState(p_options_toggle_1) )
    {
      pmodel=(pmodel | OFFSET);
    }
    else
    {
      pmodel=(pmodel & ~(OFFSET));
    }  
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 
      

  if (client_data==POINTOPTION2)
  {
    if( XmToggleButtonGetState(p_options_toggle_2) )
    {
      pmodel=(pmodel | REFRACT);
    }
    else
    {
      pmodel=(pmodel & ~(REFRACT));
    }  
    fetch_telescope_coordinates(); 
    show_telescope_coordinates();
    mark_xephem_telescope(); 
  }         

  if (client_data==POINTOPTION4)
  {
    if( XmToggleButtonGetState(p_options_toggle_4) )
    {
      pmodel=(pmodel | POLAR);
    }
    else
    {
      pmodel=(pmodel & ~(POLAR));
    }  
    fetch_telescope_coordinates(); 
    show_telescope_coordinates();
    mark_xephem_telescope(); 
  }         

  if (client_data==POINTOPTION8)
  {
    if( XmToggleButtonGetState(p_options_toggle_8) )
    {
      pmodel=(pmodel | DYNAMIC);
    }
    else
    {
      pmodel=(pmodel & ~(DYNAMIC));
    }  
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  }         

  /* If reference to target detected, then update offset */

  if (client_data==REFTARGET)
  {
    target_telescope_reference();
    usleep(1000000); 
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 
  
  /* If reference to WCS detected, then update offset */

  if (client_data==REFWCS)
  {
    wcs_telescope_reference();
    usleep(1000000);
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 
  
  /* If reference clear detected, then update offset */

  if (client_data==REFCLEAR)
  {
    clear_telescope_reference();
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 
  
  /* If reference save detected, write to disk  then update display */
  /* Default is set to 0. on startup then reset by the configuration file */

  if (client_data==REFSAVE)
  {
    save_telescope_reference();
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 
  
  /* If reference recall detected, read  then update display */
  /* Default is set to 0. on startup then reset by the configuration file */

  if (client_data==REFRECALL)
  {
    recall_telescope_reference();
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  }  

  /* If reference default detected, then update offset */
  /* Default is set to 0. on startup then reset by the configuration file */

  if (client_data==REFDEFAULT)
  {
    default_telescope_reference();
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 

  /* If model edit is requested, then handle that here */
  
  if (client_data==MODELEDIT)
  {
    /* Make the model edit panel appear */
    
    XtManageChild(model_edit);    
  }         


  /* If model clear detected, then update offset */

  if (client_data==MODELCLEAR)
  {
    clear_telescope_model();
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 
  
  /* If model save detected, write to disk  then update display */
  /* Default is set to 0. on startup then reset by the configuration file */

  if (client_data==MODELSAVE)
  {
    save_telescope_model();
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 
  
  /* If model recall detected, read  then update display */
  /* Default is set to 0. on startup then reset by the configuration file */

  if (client_data==MODELRECALL)
  {
    recall_telescope_model();
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  }  

  /* If model default detected, then update offset */
  /* Default is set to 0. on startup then reset by the configuration file */

  if (client_data==MODELDEFAULT)
  {
    default_telescope_model();
    fetch_telescope_coordinates(); 
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  } 
  
           
  /* Else noop */   
   
  if (client_data==CANCEL);
}

/* Create the unmanaged mount model edit panel */

void setup_model_edit()
{
  Arg al[10];
  int ac;
  
  ac=0;
  
  XtSetArg(al[ac], XmNselectionLabelString,
    XmStringCreateLtoR("Enter mount model: ",char_set)); ac++;
  XtSetArg(al[ac], XmNdialogTitle,
    XmStringCreateLtoR("Edit mount model",char_set)); ac++;
  model_edit = XmCreatePromptDialog(toplevel,"model_edit",al,ac);
  XtAddCallback(model_edit, XmNokCallback, model_editCB, (XtPointer) OK);
  XtAddCallback(model_edit, XmNcancelCallback, model_editCB, (XtPointer) CANCEL);
  XtUnmanageChild (XmSelectionBoxGetChild (model_edit, XmDIALOG_HELP_BUTTON));
}

/* Callback function for the mount model edit box */
/* Read mount model in units of pixels/hour       */
/* Units conversion done in pointing utility */
     
void model_editCB(w,client_data,call_data)
  Widget w;
  int client_data;
  XmSelectionBoxCallbackStruct  *call_data;
{    
  char *s;
  int testflag;
  double tmpmodelha1, tmpmodeldec1;
  
  switch (client_data)
  {
    case OK:
      XmStringGetLtoR(call_data->value,char_set,&s);
      testflag = sscanf(s, "%lf %lf ", &tmpmodelha1, &tmpmodeldec1);
      if (testflag > 0)
      {
        modelha1  = tmpmodelha1;        
        modeldec1 = tmpmodeldec1;
        modelha0  = (LSTNow() - telra);
        modeldec0 = teldec;
      }
            
      XtFree(s);
      break;
    case CANCEL:
       break;
  }
  XtUnmanageChild(w);
}


/* Callback function for the target input box */

void dialogCB(w,client_data,call_data)
  Widget w;
  int client_data;
  XmSelectionBoxCallbackStruct  *call_data;
{    
  char *s;
  double tmpra;
  double tmpdec;
  double targetra2;
  double targetdec2;
  int testflag;
  
  /* Find Epoch J2000.0 coordinates for current target target */

  tmpra = targetra;
  tmpdec = targetdec;
  Apparent(&tmpra, &tmpdec, -1);
  targetra2 = tmpra;
  targetdec2 = tmpdec;  	  


  switch (client_data)
  {
    case OK:
      XmStringGetLtoR(call_data->value,char_set,&s);
      switch (target)
      {  
        case RA: 
        testflag = dmstod(s,&tmpra);
	if (testflag == 0)
	{
	  if (display_targetepoch == EOD)
	  {
	    /* Epoch of date coordinates apply to entry */
            targetra = tmpra;
	    tmpdec = targetdec;
	    Apparent(&tmpra, &tmpdec, -1);
	    targetra2 = tmpra;
	    targetdec2 = tmpdec;	    
	  }
	  else
	  {
	    /* Epoch 2000.0 coordinates apply to entry */
	    targetra2 = tmpra;
	    tmpdec = targetdec2;
	    Apparent(&tmpra, &tmpdec, 1);
	    targetra = tmpra;
	    targetdec = tmpdec;
	  }
	}
        break;
        
        case DEC: 
        testflag = dmstod(s,&tmpdec);
	if (testflag == 0)
	{	
	  if (display_targetepoch == EOD)
	  {
	    /* Epoch of date coordinates apply to entry */  
	    targetdec = tmpdec; 
	    tmpra = targetra;
	    Apparent(&tmpra, &tmpdec, -1);	    	  
	    targetra2 = tmpra;
	    targetdec2 = tmpdec;	    
	  }
	  else
	  {
	    /* Epoch 2000.0 coordinates apply to entry */ 
	    targetdec2 = tmpdec;   
	    tmpra = targetra2;
	    Apparent(&tmpra, &tmpdec, 1);
	    targetra = tmpra;
	    targetdec = tmpdec;	         
	  }
	}
        break;
        
      }
      XtFree(s);
      break;
    case CANCEL:
       break;
  }
  show_target_coordinates();
  mark_xephem_target(); 
    
  XtUnmanageChild(w);
}
  



/* Callback function for the speed toggle buttons */

void speedCB(w,client_data,call_data)
    Widget w;
    int client_data;
    XmAnyCallbackStruct *call_data;
{
  int n;

  /* identify the button code*/
  n=client_data;
     
  /* assign a speed code */

   switch (n) {
  
     /* Guide */
     case 0: 
       telspd=SLEW;
       break; 
        
     /* Center */
     case 1: 
       telspd=FIND;
       break;
         
     /* Find */
     case 2: 
       telspd=CENTER;
       break; 
      
     /* Slew */
     case 3: 
       telspd=GUIDE;
       break;  
   } 
   SetRate(telspd);
}

/* Callback function for the 9 grid buttons. */
/* Called when one of the grid buttons is pressed. */

void buttonCB(w,client_data,call_event)
  Widget w;
  int client_data;
  XEvent *call_event;
{
  int n;
  int et = call_event->type;
  int dn,up;
  static double dha = 0.;
  static double ddec = 0.;
   
  /* identify the button number n = 3*x+y */
  n=client_data;

  /* identify the action */
  
  dn = et == ButtonPress;
  up = et == ButtonRelease;
      
  /* respond to the request */

  if(dn)
  {

    switch (n) 
    {
   
      /* NoOp */
      case 0:
      break;

      /* E */
      case 1: 
      if (guideflag == TRUE)
      {
        dha = 1.0;
        ddec = 0.0;
        strcpy(message,"Guide east\n");
        show_message();       
      }
      else
      {
        teldir=EAST;
        StartSlew(teldir);
        strcpy(message,"Move east\n");
        show_message();
      }
      break;

      /* NoOp */
      case 2: 
      break;

      /* N */
      case 3: 
      if (guideflag == TRUE)
      {
        ddec =  -1.0;
        dha = 0.0;
        strcpy(message,"Guide north\n");
        show_message();       
      }
      else
      {
        teldir=NORTH;
        StartSlew(teldir);
        strcpy(message,"Move north\n");
        show_message();
      }
      break;

      /* NoOp */
      case 4:
      break;

      /* S */
      case 5: 
      if (guideflag == TRUE)
      {
        ddec = 1.0;
        dha = 0.0;
        strcpy(message,"Guide south\n");
        show_message();       
      }
      else
      {
        teldir=SOUTH;
        StartSlew(teldir);       
        strcpy(message,"Move south\n");
        show_message();
      }  
      break;

      /* NoOp */
      case 6:
      break;

      /* W */
      case 7:
      if (guideflag == TRUE)
      {
        dha =  -1.0;
        ddec = 0.0;
        strcpy(message,"Guide west\n");
        show_message();       
      }
      else
      {
        teldir=WEST;
        StartSlew(teldir);              
        strcpy(message,"Move west\n");
        show_message();
      }
      break;

      /* NoOp */
      case 8: 
      break;    
    }
  }
  
  if(up)
  {
    if (guideflag == TRUE)
    {
            
      /* Update dynamic zero point for current guide center */
      
      modelha0 = Map12(LSTNow() - guidera);
      modeldec0 = guidedec;

      /* Change declination units from pixels to degrees */
      
      ddec = arcsecperpix*ddec/3600.;
      
      /* Add the declination correction to the permanent offsetdec in degrees */
      
      offsetdec = Map180(offsetdec + ddec);
     
      /* Change hour angle units from pixels to hours */
      
      dha = arcsecperpix*dha/54000.;

      /* Add the ha correction to the permanent offsetha in hours */

      offsetha = Map12(offsetha + dha);
           
      /* Insert a short delay to avoid too many unserviced clicks */
      
      usleep(250000.);
      
      /* In guide mode the telescope will "pull" back to guide center */

      /* Update the console message */
      strcpy(message,"Guiding\n");
      show_message();
      
    }
    else
    {  
    
      StopSlew(teldir);
      
      /* Wait for the telescope to come to rest */
      usleep(250000.*((double) telspd));
      
      /* Start tracking */
      StartTrack();

      /* Update the console message */
      strcpy(message,"Tracking\n");
      show_message();
 
      /* Display current coordinates */
      fetch_telescope_coordinates();
      show_telescope_coordinates();
      mark_xephem_telescope();
    }
  }
}

/* Callback function for the 8 telescope data command buttons */

void teldataCB(w,client_data,call_data)
  Widget w;
  int client_data;
  XmAnyCallbackStruct *call_data;
{
   
  int n, cmd_number;

  /* Identify the button */
  cmd_number=client_data;

  /* Respond to the request */
  n=cmd_number;

  switch (n) 
  {

    /* Read telescope and update mark on XEphem */
    case 0: 
      fetch_telescope_coordinates(); 
      show_telescope_coordinates(); 
      mark_xephem_telescope();
      break;

    /* Mark target on XEphem */
    case 1: 
      mark_xephem_target();
      break;

    /* Read telescope and update mark on XEphem */
    case 2: 
      fetch_telescope_coordinates(); 
      show_telescope_coordinates(); 
      mark_xephem_telescope();
      break;

    /* Read telescope and update mark on XEphem */
    case 3: 
      fetch_telescope_coordinates(); 
      show_telescope_coordinates(); 
      mark_xephem_telescope();
      break;
      
    /* Update telescope display epoch */
    case 4: 
      display_telepoch = display_telepoch + 1;
      display_telepoch = display_telepoch % 2;
      fetch_telescope_coordinates();
      show_telescope_coordinates();
      mark_xephem_telescope();
      break;        
      
    /* Enter target RA from the console */
    case 5: 
      new_target_ra(); 
      break;

    /* Enter target Dec from the console */
    case 6: 
      new_target_dec();
      break;

    /* Update target display epoch */
    case 7: 
      display_targetepoch = display_targetepoch + 1;
      display_targetepoch = display_targetepoch % 2;
      show_target_coordinates();
      break;
  }
}

/* Callback function for the 8 telescope command buttons */

void commandCB(w,client_data,call_data)
  Widget w;
  int client_data;
  XmAnyCallbackStruct *call_data;

{
   
  int n, cmd_number;

  /* identify the button */
  cmd_number=client_data;

  /* respond to the request */
  n=cmd_number;

  switch (n) 
  {

    /* Slew to target */
    case 0: 
      guideflag = FALSE;
      guideraflag = FALSE;
      guidedecflag = FALSE;
      slew_telescope(); 
      fetch_telescope_coordinates(); 
      show_telescope_coordinates(); 
      mark_xephem_telescope();
      break; 

    /* Save telescope coordinates to the log and current files */
    case 1: 
      save_coordinates();
      break;

    /* Tracking on */
    case 2: 
      StartTrack();
      gotoflag = FALSE;
      guideflag = FALSE;
      guideraflag = FALSE;
      guidedecflag = FALSE;
      strcpy(message,"Tracking\n");    
      show_message();        
      break; 
      
    /* Recall a target from the log */
    case 3: 
      recall_coordinates();
      break;
      
    /* Precision closed loop guide */
    case 4: 
      gotoflag = FALSE;
      fetch_telescope_coordinates();
      show_telescope_coordinates();
      mark_xephem_telescope();
      guidera = telra;
      guidedec = teldec;
      modelha0 = (LSTNow() - guidera);
      modeldec0 = guidedec;
      guideflag = TRUE;
      guideraflag = TRUE;
      guidedecflag = TRUE;
      strcpy(message,"RA and Dec Guiding\n");    
      show_message();              
      break;  
    
    /* Target meridian at this Dec */
    case 5: 
      fetch_telescope_coordinates();
      show_telescope_coordinates();
      mark_xephem_telescope();
      targetra = LSTNow();
      targetdec = teldec;
      show_target_coordinates();
      break; 

    /* Stop all motion including tracking */
    case 6: 
      FullStop();
      gotoflag = FALSE;
      fetch_telescope_coordinates();
      show_telescope_coordinates();
      mark_xephem_telescope();
      strcpy(message,"All motion stopped\n");    
      show_message();      
      break; 

    /* Target park position */
    case 7: 
      fetch_telescope_coordinates();
      show_telescope_coordinates();
      mark_xephem_telescope();
      targetra = Map24(LSTNow() + parkha);
      targetdec = parkdec;
      show_target_coordinates();             
      break;        
  }
}

/* Callback function for selecting the log file name */

void selectlogCB(w,client_data,call_data)
  Widget w;
  int client_data;
  XmAnyCallbackStruct *call_data;
 
{
  XmFileSelectionBoxCallbackStruct *s =
      (XmFileSelectionBoxCallbackStruct *) call_data;
  
  /* Do nothing if cancel is selected. */
  if (client_data==CANCEL) 
  {
      XtUnmanageChild(select_logfile);
      return;
  }

  /* Assign a new file name if ok is selected. */
  if (client_data==OK) 
  {
     /* Wipe the old file name */
     
     logfile[0] = '\0';
     
     /* Get the new file name from the file selection box */
     
     XmStringGetLtoR(s->value, char_set, &logfile);      
     XtUnmanageChild(select_logfile);
     return;
  }
  
  /* Handle unexpected client data */
  
  XtUnmanageChild(select_logfile);


} 
 
/* Callback function for selecting the configuration file name */

void selectconfigCB(w,client_data,call_data)
  Widget w;
  int client_data;
  XmAnyCallbackStruct *call_data;
  
{
  XmFileSelectionBoxCallbackStruct *s =
      (XmFileSelectionBoxCallbackStruct *) call_data;
  
  /* Do nothing if cancel is selected. */
  if (client_data==CANCEL) 
  {
     XtUnmanageChild(select_configfile);
     return;
  }

  /* Assign a new file name if ok is selected. */
  if (client_data==OK) 
  {
     /* Wipe the old file name */
     
     configfile[0] = '\0';
     
     /* Get the new file name from the file selection box */
     
     XmStringGetLtoR(s->value, char_set, &configfile);   
     
     /* Read the new configuration */
        
     read_config();
     
     /* Close the box */
     
     XtUnmanageChild(select_configfile);
     return;
  }
  
  /* Handle unexpected client data */
  
  XtUnmanageChild(select_configfile);
} 
    
/* Callback function for selecting and reading the queue file  */
 
void readqueueCB(w,client_data,call_data)
  Widget w;
  int client_data;
  XmAnyCallbackStruct *call_data;
{
  XmFileSelectionBoxCallbackStruct *s =
      (XmFileSelectionBoxCallbackStruct *) call_data;
  
  /* Do nothing if cancel is selected. */
  if (client_data==CANCEL) 
  {
    XtUnmanageChild(read_queuefile);
    return;
  }

  /* Manage queue file name */
  if(queuefile != NULL)
  {
    /* XtFree(queuefile); */
    queuefile = NULL;
  }
  
  /* Get the filename from the file selection box */
  XmStringGetLtoR(s->value, char_set, &queuefile);

  /* Read the queue into memory */
  read_queue();

  XtUnmanageChild(read_queuefile);
}    


/* Callback function for selecting a queue entry */

void selectqueueCB(w,client_data,call_data)
  Widget w;
  XtPointer client_data;
  XtPointer call_data;
{
  double targetra2;
  double targetdec2;

  XmListCallbackStruct *cbs = (XmListCallbackStruct *) call_data;

  /* Selection is indexed from 1 in XmList and from 0 in our list */
  
  queuechoice =  cbs->item_position - 1;    
    
  if (!quiet)
  {
    printf(" Queue entry #%d: \n",queuechoice);                                          
    printf("   %s\n",queue[queuechoice].name);    
    printf("   ra: %lf  dec: %lf\n\n",                             
    queue[queuechoice].ra,queue[queuechoice].dec);  
  }

  targetra2 = queue[queuechoice].ra;
  targetdec2 = queue[queuechoice].dec;
  targetra = targetra2;
  targetdec = targetdec2;
  Apparent(&targetra, &targetdec, 1);
  show_target_coordinates();
  mark_xephem_target(); 
}    



/* Update message area of user interface */

void show_message()
{
  XmString xmstr;
  
  xmstr = XmStringCreateLtoR(message,char_set);
  XmTextSetString(message_area, message); 
  XmStringFree(xmstr);
} 


/* Show telescope coordinates for telepoch in the button */

void show_telescope_coordinates()
{
  double tmpra, tmpdec;
  XmString xmstr;
  Arg al[10];
  int ac;

  switch (display_telepoch)
  {
          
    case J2000:
    
      /* Display Epoch 2000.0 coordinates */      
    
      xmstr = XmStringCreateLtoR("2000.0",char_set);
      ac=0; 
      XtSetArg(al[ac],XmNlabelString,xmstr); ac++; 
      XtSetValues(get_telepoch,al,ac);
      tmpra=telra;
      tmpdec=teldec;         
      Apparent(&tmpra,&tmpdec,-1);
      break;
    
    case  EOD:
    
      /* Display EOD coordinates */      
    
      xmstr = XmStringCreateLtoR("Now",char_set);
      ac=0; 
      XtSetArg(al[ac],XmNlabelString,xmstr); ac++; 
      XtSetValues(get_telepoch,al,ac);
      tmpra=telra;
      tmpdec=teldec; 
      break;
  
  }   

  dtodms(buf, &tmpra);
  xmstr = XmStringCreateLtoR(buf,char_set);
  ac=0;
  XtSetArg(al[ac],XmNlabelString,xmstr); ac++;
  XtSetValues(get_telra,al,ac);
  
  dtodms(buf, &tmpdec);
  xmstr = XmStringCreateLtoR(buf,char_set);
  ac=0;
  XtSetArg(al[ac],XmNlabelString,xmstr); ac++;
  XtSetValues(get_teldec,al,ac);    

  XmStringFree(xmstr);
  
}  
  
/* Show target coordinates for targetepoch in the button */

void show_target_coordinates()
{
  double tmpra, tmpdec;
  XmString xmstr;
  Arg al[10];
  int ac;
  
  switch (display_targetepoch)
  {
    
    case J2000:
    
      /* Display Epoch 2000.0 coordinates.                                */
      /* If XEphem transfered proper motion, then the target coordinates  */
      /*   stored in XmTel include proper motion to the current date.     */
      /*   Coordinates including proper motion and precessed to 2000.0    */
      /*   will differ from an epoch 2000.0 catalog entry that            */
      /*   does not include proper motion to the current date.            */
    
      ac=0;      
      xmstr = XmStringCreateLtoR("2000.0",char_set);
      XtSetArg(al[ac],XmNlabelString,xmstr); ac++; 
      XtSetValues(get_targetepoch,al,ac);
      tmpra=targetra;
      tmpdec=targetdec;        
      Apparent(&tmpra,&tmpdec,-1);
      break;
    
    case  EOD:
    
      /* Display EOD coordinates as saved in targetra and targetdec. */
    
      xmstr = XmStringCreateLtoR("Now",char_set);
      ac=0; 
      XtSetArg(al[ac],XmNlabelString,xmstr); ac++; 
      XtSetValues(get_targetepoch,al,ac);
      tmpra=targetra;
      tmpdec=targetdec;          
      break;
  
  }      

  dtodms(buf, &tmpra);
  xmstr = XmStringCreateLtoR(buf,char_set);
  ac=0;
  XtSetArg(al[ac],XmNlabelString,xmstr); ac++;
  XtSetValues(get_targetra,al,ac);   

  dtodms(buf, &tmpdec);
  xmstr = XmStringCreateLtoR(buf,char_set);
  ac=0;
  XtSetArg(al[ac],XmNlabelString,xmstr); ac++; 
  XtSetValues(get_targetdec,al,ac);       

  XmStringFree(xmstr);
}  

void poll_interval_handler(XtPointer client_data_ptr, XtIntervalId *client_id) 
{
  static int tcount; 
  
  /* Manage user inferface updates promptly */ 
  
  show_guide_status();
  show_message();

  /* Show low priority updates every minute */

  if ((tcount < 0) || (tcount > 60)) 
  {
    tcount = 1;
  }
  tcount++;   
            
  /* Check the slew status */

  if ( gotoflag == TRUE )
  {
     check_slew_status();
  } 
  
  /* Update the precision guiding option */
  
  if ( guideflag == TRUE )
  {
    CenterGuide(guidera, guidedec,  guideraflag, guidedecflag, pmodel);
  } 
  
  /* Start the timer again.  It will return to the handler after poll_interval. */

  poll_interval_id = XtAppAddTimeOut(context, poll_interval,
    poll_interval_handler, poll_interval_data_ptr);
}  


/* Slew telescope to target coordinates, display coords, update message */

void slew_telescope()
{

  gotoflag=GoToCoords(targetra,targetdec,pmodel);
  if ( gotoflag == FALSE )
  {
    strcpy(message,"Slew request completed\n");
    show_message();
    return;
  }
    
  /* Pause here to allow telescope CPU to process slew request */
  
  usleep(2000000.);
  return;
}

void check_slew_status()
{
  int status;
  
  status = CheckGoTo(targetra,targetdec,pmodel);
  
  if ( status < 1 )
  {
    strcpy(message,"Slew in progress\n");
    show_message(); 
    gotoflag = TRUE;
    fetch_telescope_coordinates(); 
    show_telescope_coordinates();  
    mark_xephem_telescope();     
  }
  else if ( status == 1 )
  { 
    strcpy(message,"Target acquired\n");
    show_message(); 
    fprintf(stderr,"Starting track again\n");
    StartTrack();
    strcat(message,"Tracking\n");    
    show_message();
    gotoflag = FALSE;
    fetch_telescope_coordinates();
    write_coords(telra, teldec);
    show_telescope_coordinates(); 
    mark_xephem_telescope();
  }
  else
  {
    strcpy(message,"Unexpected slew status\n");
    show_message(); 
    fprintf(stderr,"Starting track again\n"); 
    StartTrack();
    strcat(message,"Tracking\n");    
    show_message(); 
    gotoflag = FALSE; 
    fetch_telescope_coordinates();   
    write_coords(telra, teldec);
    show_telescope_coordinates(); 
    mark_xephem_telescope();  
  }   
  return;
}         

/* Update reference to current target                                      */

/* This is actually more complex than a simple offset correction if        */
/* a pointing model is in place.  In that case, the offset should be       */
/* to the base ra and dec reported by the telescope, taken such that       */
/* the actual ra and dec with offset and pointing correction will          */
/* agree with the reference target.  It will have to iterate to achieve    */
/* this effect.  The simple difference here from the reported coordinates  */
/* with other pointing model corrections still in place will impact        */
/* the effect of the pointing model.                                       */

void target_telescope_reference(void)          
{    

  double tmpoffsetha, tmpoffsetdec;

  /* Turn off reference correction for a moment by setting offsets to zero */
  
  tmpoffsetha = offsetha;
  tmpoffsetdec = offsetdec;
  offsetha = 0.;
  offsetdec = 0.;
  
  /* Get the coordinates with other pointing corrections in place  */
    
  fetch_telescope_coordinates();
  
  /* Trap the very confusing errors that would happen in an offset */
  /* assigned across the poles.                                    */

  if ( fabs(teldec)> 85. )
  {
    offsetha = tmpoffsetha;
    offsetdec = tmpoffsetdec; 
    strcpy(message,
      "Telescope reports a declination within 5 degrees of the poles.\n");
    strcat(message,"Original internal offsets remain in use.\n");
    show_message();
    return;
  }

  if (fabs(targetdec)> 85. )
  {
    offsetha = tmpoffsetha;
    offsetdec = tmpoffsetdec; 
    strcpy(message,
      "Reference is not permitted to a target within 5 degrees of the poles.\n");
    strcat(message,"Original offsets remain in use.\n");
    show_message();
    return;
  } 
  
  /* Calculate new additive offsets */
    
  offsetha =  Map12(telra - targetra);
  offsetdec = targetdec - teldec;
    
  /* Check that the offsets are not too large */
  
  if (fabs(offsetdec) >= 15.0)
  {
    offsetdec = tmpoffsetdec;
    offsetha = tmpoffsetha;
    strcpy(message,
      "New offset in declination would be greater than 15 degrees.\n");
    strcat(message,"Original offsets remain in use.\n");
    show_message();
    return;
  }
  
  if (fabs(offsetha) >= 1.0)
  {
    offsetdec = tmpoffsetdec;
    offsetha = tmpoffsetha;
    strcpy(message,
      "New offset in hour angle would be greater than 1 hour.\n");
    strcat(message,"Original offsets remain in use.\n");
    show_message();
    return;
  } 

  strcpy(message,
    "New offsets: \n");
  sprintf(message2,"HA: ");
  strcat(message,message2);
  dtodms(message2,&offsetha); 
  strcat(message, message2);
  strcat(message,"\n");
  sprintf(message2,"Dec: ");
  strcat(message,message2);
  dtodms(message2,&offsetdec); 
  strcat(message, message2);
  strcat(message,"\n");  
  show_message();
  
  fprintf(stderr,"Telescope: RA %lf  Dec %lf\n",telra,teldec);
  fprintf(stderr,"Target:    RA %lf  Dec %lf\n",targetra,targetdec);
  fprintf(stderr,"Offsets:   HA %lf  Dec %lf\n",offsetha,offsetdec); 
}

/* Write telescope reference  to a system file */

void save_telescope_reference()
{  
  FILE* outfile;
  outfile = fopen("/usr/local/observatory/status/teloffsets","w");
  if ( outfile == NULL )
  {
    fprintf(stderr,"Cannot update teloffsets file\n");
    return;
  }

  fprintf(outfile, "%lf %lf\n", offsetha, offsetdec);      
  fclose(outfile);
}
 
/* Recall telescope reference from a system file */

void recall_telescope_reference()
{
  FILE* fp_offsets;
  double tmpoffsetha, tmpoffsetdec;
  int recall_flag;
  fp_offsets = fopen("/usr/local/observatory/status/teloffsets","r");
  if ( fp_offsets == NULL )
  {
    fprintf(stderr,"Cannot open a teloffsets file\n");
    return;
  }
  recall_flag = fscanf(fp_offsets, "%lf %lf", &tmpoffsetha, &tmpoffsetdec);
  if (recall_flag == 2)
  {
    offsetha = tmpoffsetha;
    offsetdec = tmpoffsetdec;
    fprintf(stderr, "%lf %lf\n", offsetha, offsetdec);      
  }
  fclose(fp_offsets);
}

/* Read wcs coordinates and compute new offsets */
  
void wcs_telescope_reference()
{
  strcpy(message, "Reference from a WCS header is not yet implemented.\n");
  show_message();
  fprintf(stderr, "Reference from a WCS header is not yet implemented.\n");
}
  
/* Use default offsets */
  
void default_telescope_reference(void)
{
  offsetha = offsetha_default;
  offsetdec = offsetdec_default;
  fprintf(stderr, "Recalled default offsets: %lf %lf\n", offsetha, offsetdec);      
}

/* Clear telescope offsets */

void clear_telescope_reference(void)
{
  offsetha = 0.;
  offsetdec = 0.;
  fprintf(stderr,"New offsets:  %lf  %lf\n",offsetha,offsetdec); 
  return;
}


/* Write telescope model  to a system file */
/* Output units are arcseconds per pixel */

void save_telescope_model()
{  
  FILE* outfile;
  outfile = fopen("/usr/local/observatory/status/telmodel","w");
  if ( outfile == NULL )
  {
    fprintf(stderr,"Cannot update telmodel file\n");
    return;
  }
  fprintf(outfile, "%lf %lf\n", modelha1, modeldec1);      
  fclose(outfile);
}
 
/* Recall telescope model from a system file */
/* Input units are arcseconds per pixel */

void recall_telescope_model()
{
  FILE* infile;
  int recall_flag;
  infile = fopen("/usr/local/observatory/status/telmodel","r");
  if ( infile == NULL )
  {
    fprintf(stderr,"Cannot open a telmodel file\n");
    return;
  }
  recall_flag = fscanf(infile, "%lf %lf", &modelha1, &modeldec1);
  if (recall_flag == 2)
  {
    fprintf(stderr, "Recalled mount model parameters %lf %lf\n", modelha1, modeldec1);    
    modelha0 = (LSTNow() - telra);    
    modeldec0 = teldec;
  }
  fclose(infile);
}
  
/* Use default model */
  
void default_telescope_model(void)
{
  modelha0  = Map12(LSTNow() - telra);
  modelha1  = modelha1_default;
  modeldec0 = teldec;
  modeldec1 = modeldec1_default;
  
  fprintf(stderr, "Recalled default model\n");      
}

/* Clear telescope model */

void clear_telescope_model(void)
{
  modelha0  = 0.;
  modelha1  = 0.;
  modeldec0 = 0.;
  modeldec1 = 0.;
  
  fprintf(stderr,"Model parameters cleared\n");
  return;
}


/* Export current telescope coordinates to XEphem */

void mark_xephem_telescope()     
{
  double tmpra, tmpdec;
  double rarad,decrad;
  char outbuf[101];
  
  if (fd_fifo_out>=0)
  { 
  
    /* Recall the telescope's EOD coordinates */
    
    tmpra=telra;
    tmpdec=teldec;         
    
    /* Convert to 2000.0 */
    
    Apparent(&tmpra,&tmpdec,-1);   
    
    /* Convert to radians */
    rarad=tmpra*PI/12.;  
    decrad=tmpdec*PI/180.; 
    
    /* Send the values to the fifo */
    
    snprintf(outbuf,sizeof(outbuf),"RA:%9.6f Dec:%9.6f Epoch:2000.000\n",
      rarad,decrad);  
    write(fd_fifo_out, outbuf, strlen(outbuf));
  }
}

/* Export current target coordinates to XEphem */

void mark_xephem_target()     
{
  double tmpra, tmpdec;
  double rarad,decrad;
  char outbuf[101]; 
  
  /* Recall the telescope's EOD coordinates */
  
  tmpra=targetra;
  tmpdec=targetdec;         
  
  /* Convert to Epoch 2000.0  coordinates */
  
  Apparent(&tmpra,&tmpdec,-1);   
  
  /* Convert to radians */
  
  rarad=tmpra*PI/12.;  
  decrad=tmpdec*PI/180.; 
  
  /* Send the values to the fifo */  
  
  snprintf(outbuf,sizeof(outbuf),"RA:%9.6f Dec:%9.6f Epoch:2000.000\n",
    rarad,decrad);  
  write(fd_fifo_out, outbuf, strlen(outbuf));
}

/* Read XEphem goto fifo and parse target coordinates */

void read_xephem_target() 
{
  int i,j,k;
  double rahr,ramin,rasec;
  double decdeg,decmin,decsec;
  double tmpra,tmpdec,tmpepoch;
  double tmp_pm_ra=0.;
  double tmp_pm_dec=0.;
  double tmpdat;
  char fifostr[121];
  char rastr[121];
  char *raptr=rastr;  
  char inbuf[2];
  
  /* A typical fixed target string is:                            */

  /* Regulus,f|M|B7,10:08:22.3, 11:58:02,  1.35,2000,0            */
  /* Name, type info, RA | pm, Dec | pm, Mag, Epoch, Identifier   */

  /* Return if the buffer is not open                             */

  if(fd_fifo_in<0)
    return;

  /* Read the buffer */

  k=0;
  while(1) {
    i = read(fd_fifo_in,inbuf,1);
    if(i==-1) {
      fifostr[k]='\0';
      break;
    }
    if(k<120)
    fifostr[k++]=inbuf[0];
  }
  
  /* Diagnostic of fifo string contents */
  
  /*  printf("Fifo in: %s\n",fifostr); */
    
  /* Examples from XEphem version 3.6.xx -- */
  
  /* Arcturus,f|V|K1,14:15:39.7|-1093, 19:10:57|-1998,-0.04,2000,0  */
  /* HD 124953,f|V|A8,14:16:04.2|43, 18:54:43|-28,5.98,2000,0       */
  /* Crt Delta-12,f|S|G8,11:19:20.5|-122,-14:46:43|208,3.56,2000,0  */
  /* GSC 0838-0788,f|S,10:32:32.4,  9:06:17,13.84,2000,0            */
  /* M67,f|O|T2, 8:51:24.0, 11:49:00,6.90,2000,1500|1500|0          */
  /* NGC 2539,f|O|T2, 8:10:36.9,-12:49:14,6.50,2000,900|900|0       */
  /* Venus,P                                                        */


  /* If the buffer was not empty then extract its contents */

  if (k>1) 
  {
    fifostr[k]='\0';

    /* Remove any white space */

    i=0;
    j=0;
    while (i<=k)
    {
      if( fifostr[i]!=' ' )
      {
        rastr[j]=fifostr[i];
        j++;
      }
      i++;
    }  
    
    /* Diagnostic of clean coordinate string contents */

    /*  printf("Coordinate str: %s\n",rastr); */
      

    /* Skip the name and description fields by searching for delimiting commas */
    /* This will skip orbital objects for which XEphem does not pass RA and Dec */   

    raptr=rastr;
    i=0;
    while(i<2)  
    {
      raptr=strchr(raptr,',');
      if(raptr==NULL) break;
      raptr++;
      i++;
    }
    
    
    /* Parsing will have proceeded to the RA field if this is a catalog object */

    if(i==2)
    { 

      /* Scan and save RA data */ 
      /* Note that the XEphem proper motion is in milliarcseconds per year */ 
      /* Our pm_ra is in seconds of time per year */

      tmpdat=0.;
      j=sscanf(raptr,"%lf:%lf:%lf|%lf", 
        &rahr,&ramin,&rasec,&tmpdat);

      if(j>0) 
      {
        tmpra = rahr + ramin/60. + rasec/3600.;
        tmp_pm_ra=tmpdat/15000.;
      }
       
      /* Try to advance pointer to the Dec field */

      raptr=strchr(raptr,',');
      if(raptr != NULL)
      {
       raptr++;
       i++;
      }
    }
     

    if(i==3)
    {  

      /* Now pointing at the Dec field so scan and save Dec data */
      /* Note that the XEphem proper motion is in milliarcseconds per year */ 
      /* Our pm_dec is in seconds of arc per year */

      tmpdat=0.;
      j=sscanf(raptr,"%lf:%lf:%lf|%lf", 
        &decdeg,&decmin,&decsec,&tmpdat);

      if(j>0) 
      {
        if(decdeg<0.)
        {
          tmpdec = decdeg - decmin/60. - decsec/3600.;
        }
        else
        { 
          tmpdec = decdeg + decmin/60. + decsec/3600.;
        }
        tmp_pm_dec=tmpdat/1000.;
      }     

      /* Try to advance pointer to the Magnitude field */

      raptr=strchr(raptr,',');
      if(raptr != NULL)
      {
       raptr++;
       i++;
      }
            
    }

    if(i==4)
    {  

      /* Now pointing at the Magnitude field so scan past it */ 

      j=sscanf(raptr,"%lf", &tmpdat);

      /* Magnitude is not used later but if(j>0) magnitude is tmpdat */
      
      /* Try to advance pointer to the Epoch field */

      raptr=strchr(raptr,',');
      if(raptr != NULL)
      {
       raptr++;
       i++;
      }   
    }
  
    if(i==5)
    {  

      /* Now pointing at the Epoch field so scan and show Epoch data */ 

      j=sscanf(raptr,"%lf", &tmpepoch);

      if(j>0) 
      {

         /* The catalog entry epoch has been found */
         /* Apply proper motion to EOD */

         ProperMotion(tmpepoch, &tmpra, &tmpdec, tmp_pm_ra, tmp_pm_dec);        
      
         /* Find the apparent coordinates including */
         /*   precession, nutation, and stellar aberration for the EOD */

         if ((tmpepoch > 1999.99) && (tmpepoch < 2000.01)) 
         {
           
           /* Epoch 2000.0 coordinates apply */
           /* Find the apparent position of the object for EOD */
           
           targetra = tmpra;
           targetdec = tmpdec;
           Apparent(&targetra, &targetdec, 1);
         }
         else 
         {
         
           /* Some other epoch applies.  Caution!!  */
           /* Set the epoch to 2000.0 and then proceed as before. */
           /* If coordinates are not FK5 this will not be accurate. */
                     
           PrecessToEOD(tmpepoch, &tmpra, &tmpdec);
           PrecessToEpoch(2000.0, &tmpra, &tmpdec);
          
           /* Epoch 2000.0 coordinates now apply */
           /* Find the apparent position of the object for EOD */

           targetra = tmpra;
           targetdec = tmpdec;
           Apparent(&targetra, &targetdec, 1);
         }
                           
         /* Show the entry at the epoch selected for display */         
         
         show_target_coordinates();
      }
      else
      {
         
         /* No epoch was found so assume EOD coordinates */
         
         targetra = tmpra;
         targetdec = tmpdec;       
         
         /* Show the entry at the epoch selected for display */
         
         show_target_coordinates();  
      }     
    }  
  }
}


/* Write current telescope coordinates to the xmtel log file and to history */

void save_coordinates()           
{
  char decstr[20], rastr[20];
  time_t timebuf;
  char *gmtstr;
  
  time(&timebuf);
  gmtstr = asctime(gmtime(&timebuf));
  if (gmtstr[strlen(gmtstr) - 1] == '\n') gmtstr[strlen(gmtstr) - 1]= '\0';

  dtodms(rastr,&telra);
  dtodms(decstr,&teldec);
  fp_log=fopen(logfile, "a");
  fprintf(fp_log,"%s  %s  %s\n",gmtstr,rastr,decstr);
  fflush(fp_log);
  fclose(fp_log);
  
  /* Expand the buffer up to a maximum of 100 entries */
  
  if (nhistory < 100) 
  {
    nhistory++;
  }
  
  /* Increment the ring counter */
  
  last_history_saved++;
  
  /* Test for wraparound and reset history counters */
  
  if (last_history_saved > 99) 
  {
    last_history_saved=0;
  }
  last_history_recalled = last_history_saved - 1;
  
  /* Save the current telescope coordinates in the last_history location */
  
  strncpy(history[last_history_saved].name,gmtstr,79);
  history[last_history_saved].ra = telra;
  history[last_history_saved].dec = teldec;

  /* Write this to the status directory too */
  
  write_coords(telra, teldec);

}

/* Read a new target from the xmtel history file */

void recall_coordinates()           
{
  
  if ( last_history_saved >= 0 )
  {
    last_history_recalled++;
    if (last_history_recalled > nhistory -1 )
    {
      last_history_recalled = 0;
    }
    targetra = history[last_history_recalled].ra;
    targetdec = history[last_history_recalled].dec;
    show_target_coordinates();
    strcpy(message,history[last_history_recalled].name);
    strcat(message,"\n");
    show_message();
  }
  else
  {
    strcpy(message,"Nothing yet saved.\n");
    show_message();
  }
}

/* Read the queue line by line */

void read_queue()
{
  char queuestr[121];
  char *queueptr=queuestr;
  int i,j,k;
  int nline;
  int success;
  double rahr,ramin,rasec;
  double decdeg,decmin,decsec;

  Arg al[10];
  int ac;
  
  strcpy(message,"Reading the queue ");
  strcat(message,queuefile);
  strcat(message,"\n");
  show_message();
  
  fp_queue=fopen(queuefile, "r");

  /* Queue is indexed from 0 so we start with -1 here */
  /* Line counter is indexed from 1 and we start with 0 here */
  
  nqueue = -1;
  nline = 0;
  while ( queuestr == fgets(queuestr,120,fp_queue) )
  {
    /* Reset a flag to determine whether to use the entry */
    
    success = FALSE;
    
    if (!quiet)
    {
    
      printf("Queue line %d: %s\n", nline, queuestr);

    }
     
    /* If the buffer is not empty and not a comment then extract its parts */

    k=strlen(queuestr);
    if ( (k>1) && ( queuestr[0] != '#' ) ) 
    {

      success = TRUE;
      nqueue++;
      nline++;
                    
      /* Null terminate the string */
      
      queuestr[k]='\0';
      queueptr=queuestr;
      
      /* The name field may have useful white space */
      
      /* Identify the entire name field and copy it to the local queue */

      j=strcspn(queueptr,",");
      strncpy(queue[nqueue].name,queueptr,j);
      queueptr = queueptr + j + 1;
      
      /* Remove any white space as the entire string is copied on itself */

      i=0;
      j=0;
      while (i<=k)
      {
        if( queuestr[i]!=' ' )
        {
          queuestr[j]=queuestr[i];
          j++;
        }
        i++;
      }  

      /* Set the pointer at the beginning of the clean string */

      queueptr=queuestr;
      i = 0;
      
      /* Skip the cleaned name field */

      queueptr=strchr(queueptr,',');
      if ((queueptr == NULL) || (queueptr == queuestr))
      {
        success = FALSE;
        i = 0;
      }
      else
      {
        i = 1;
        queueptr++;
      }
      
      if (!quiet)
      {    
        printf("Queue coordinate success flag: %d\n", success);
      }


      /* Parsing may have proceeded to the RA field if this is an object */

      if ( (i == 1) && (success == TRUE) )
      { 

        /* Scan and save RA data in a temporary buffer */ 

        j=sscanf(queueptr,"%lf:%lf:%lf", 
          &rahr,&ramin,&rasec);

        if (j>0) 
        {
          queue[nqueue].ra = rahr + ramin/60. + rasec/3600.;
          
          /* Try to advance pointer to the Dec field */

          queueptr=strchr(queueptr,',');
          if(queueptr != NULL)
          {
            queueptr++;
            i++;
          }
        }
        else
        {
          success = FALSE;
        }                  
      }
     
     
      if ( (i == 2) && (success == TRUE) )
      {  

        /* Now pointing at the Dec field so scan and save Dec data */
        /* Queue proper motion is in milliarcseconds per year */ 
        /* Our pm_dec is in seconds of arc per year */

        j=sscanf(queueptr,"%lf:%lf:%lf", 
        &decdeg,&decmin,&decsec);

        if (j>0) 
        {
          if (decdeg>0.)
          {
            queue[nqueue].dec = decdeg + decmin/60. + decsec/3600.;
          }
          else
          { 
            queue[nqueue].dec = decdeg - decmin/60. - decsec/3600.;
          }
        }
        else
        {        
          success = FALSE;
        }     
      }
               
      if (!success)
      {
        if (!quiet )
        {
          printf(" Skipping queue line %d:\n", nline);                                   
        }
        nqueue--;
      }  
      else 
      {
                                                                    
        if (!quiet )
        {
          printf(" #%d\n",nqueue);                                   
          printf(" %s\n",queue[nqueue].name);  
          printf(" ra: %lf  dec: %lf \n\n",                 
            queue[nqueue].ra, queue[nqueue].dec);   
        }
      } 
    }

  } 

  fclose(fp_queue);

  /* Increment nqueue so that it now reads the number of entries to the queue */
  
  nqueue++;

  XmStringTable queue_list =
        (XmStringTable) XtMalloc ( nqueue * sizeof (XmString) ); 
  for (i=0; i < nqueue; i++)
  {
    queue_list[i] = XmStringCreateLocalized (queue[i].name); 
  }
  ac=0; 
  XtSetArg(al[ac],XmNitemCount,nqueue); ac++; 
  XtSetArg(al[ac],XmNitems,queue_list); ac++;
  XtSetValues(queue_area,al,ac);  
  for (i = 0; i < nqueue; i++)
  {
    XmStringFree(queue_list[i]);
  }
  free(queue_list);

}


/* Import current telescope coordinates: ra, ha, and dec */

void fetch_telescope_coordinates() 
{
  if (CheckConnectTel() != FALSE)
  {
    GetTel(&telra, &teldec, pmodel);
    telha = Map12(LSTNow() - telra);
  }
}


/* User input of target RA */

void new_target_ra() 
{
  target=RA;
  XtManageChild(dialog);
}

/* User input of target Dec */

void new_target_dec()
{
  target=DEC;
  XtManageChild(dialog);
}


/* Show guide status in the message window */

void show_guide_status(void)
{
  if (guideflag == TRUE)
  {
    sprintf(message,"Guiding\n");
    show_message();
  } 
  else
  {
    sprintf(message,"Not guiding\n");
    show_message();
  }   
     
}    



/* Start telescope communications */

void link_telescope()
{
  ConnectTel();
  fflush(stdout);
  telflag=CheckConnectTel();

  /* Connection diagnostics displayed in message window and updated in menu */

  switch (telflag)
  {
            
    case TRUE:
      fprintf(stdout, "The telescope is connected. \n");
      strcpy(message, "The telescope is connected.\n");
      show_message();      
      break;
            
    case FALSE:
      fprintf(stdout, "The telescope is not connected.\n");
      strcpy(message, "The telescope is not connected.\n");
      show_message();      
      // exit(EXIT_FAILURE);     
      break;   
  }

  /* Select the startup states already marked on the control panel */

  telspd = FIND;
  SetRate(telspd);
  

  /* Retrieve and display telescope pointing information */ 

  fetch_telescope_coordinates();
  targetra=telra;
  targetdec=teldec;  
  show_telescope_coordinates();
  mark_xephem_telescope();
  show_target_coordinates();

  /* Diagnostics to test software */

  TestAlgorithms();
}

/* Stop telescope communications */

void unlink_telescope()          
{
  fflush(stdout);
  DisconnectTel();
}

       
/* Start fifo links */

void link_fifos()
{  
  fd_fifo_in = open("/usr/local/xephem/fifos/xephem_loc_fifo", 
    O_RDWR | O_NONBLOCK);


  if (fd_fifo_in<0) {
    fprintf(stderr,"Unable to open /usr/local/xephem/fifos/xephem_loc_fifo. \n");
    }
  fd_fifo_out = open("/usr/local/xephem/fifos/xephem_in_fifo", 
    O_RDWR | O_NONBLOCK);


  if (fd_fifo_out<0) {
    fprintf(stderr,"Unable to open /usr/local/xephem/fifos/xephem_in_fifo. \n");
    }
}


/* Stop fifo links */

void unlink_fifos()
{
  if (fd_fifo_in!=-1)
    close(fd_fifo_in);
  if (fd_fifo_out!=-1)
    close(fd_fifo_out);  
}

/* Convert string deg:min:sec or hr:min:sec to a double */

int dmstod (char *instr, double *datap)
{
  double h=0., m=0., s=0.;
  int negflag;
  int convertflag;
  
  while (isspace(*instr))
    instr++;
  if (*instr == '-') {
    negflag = 1;
    instr++;
  } else
    negflag = 0;
    
  convertflag = sscanf (instr,  "%lf%*[:]%lf%*[:]%lf", &h, &m, &s);
  if (convertflag < 1)
    return (-1);
  *datap = h + m/60. + s/3600.;
  if (negflag)
    *datap = - *datap;
  return (0);
}
    
/* Convert double to string deg:min:sec or hr:min:sec */
/* This routine has memory leaks that should be fixed */

void dtodms (char *outstr, double *dmsp)
{

  int d=0, m=0, s=0;
  int negflag;
  double dms, ms;
   
  dms = *dmsp;

  /* If negative, treat as positive and set sign in string */

  if (dms < 0 ) 
  {
    negflag = 1;
    dms = - dms;
  }
  else
  {
    negflag = 0;
  }

  /* Allow for truncation if input is in seconds of arc */
  
  dms = dms + 0.5/3600.;

  /* Get the whole part in degrees or hours, as needed */  

  d = (int) dms;
  
  /* Could test here for overflow at 24h or 360d          */
  /*   but we'll assume that's been done before the call. */
    
  ms = dms - (double) d;
  ms = ms*60.;
  m = (int) ms;
  s = (int) 60.*(ms - (double) m);
  if (negflag)
  {
    sprintf(outstr,"-%02d:%02d:%02d",d,m,s);
  }
  else
  {
    sprintf(outstr,"%02d:%02d:%02d",d,m,s);      
  }
}

/* Read and parse the initial configuration file */
/* Will override defaults for                    */
/*                                               */
/*   polaraz                                     */
/*   polaralt                                    */
/*   offsetha                                    */
/*   offsetdec                                   */
/*   latitude                                    */
/*   longitude                                   */
/*   altitude                                    */
/*   pressure                                    */
/*   temperature                                 */
/*   parkha                                      */
/*   parkdec                                     */
/*   telserial                                   */

/* Requires configfile defined and allocated     */


void read_config(void)
{
  char configstr[121];
  char *configptr = configstr;
  int n;
      
  fp_config = fopen(configfile, "r");

  if ( fp_config == NULL )
  {
    fprintf(stderr,"New telescope configuration not found\n");
    fprintf(stderr,"Using default telescope parameters\n");
    return;
  }
  else
  {
    fprintf(stderr,"Telescope and site parameters redefined\n");
  }
  
  while ( configstr == fgets(configstr,80,fp_config) )
  {
    configptr = strstr(configstr,"tel.mount");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%d",&telmount);
        fprintf(stderr,"Telescope mounting type: %d\n",telmount);
      }
    }

    configptr = strstr(configstr,"tel.serial");
    if ( configptr != NULL)
    {
      
      /* Assign a serial port if the telescope is not connected */
      
      if (telflag != TRUE)
      {
        configptr = strstr(configstr,"=");
        if ( configptr != NULL)
        {
          configptr = configptr + 1;
          strncpy(telserial,configptr,30);
          n = strlen(telserial);
          if (n>0)
          {
            telserial[n-1]='\0';
          }  
          fprintf(stderr,"Telescope serial port set to: %s\n",telserial);
        }
      }
    }

    configptr = strstr(configstr,"tel.homeha");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&homeha);
        fprintf(stderr,"Home HA: %lf\n",homeha);
        homenow = TRUE;
      }  
    }

    configptr = strstr(configstr,"tel.homedec");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&homedec);
        fprintf(stderr,"Home Dec: %lf\n",homedec);
        homenow = TRUE;
      }  
    }
    
    configptr = strstr(configstr,"tel.parkha");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&parkha);
        fprintf(stderr,"Park HA: %lf\n",parkha);
        homenow = TRUE;
      }  
    }

    configptr = strstr(configstr,"tel.parkdec");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&parkdec);
        fprintf(stderr,"Park Dec: %lf\n",parkdec);
        homenow = TRUE;
      }  
    }    

    configptr = strstr(configstr,"tel.polaraz");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&polaraz);
        fprintf(stderr,"Polar azimuth: %lf\n",polaraz);
      }  
    }
    
    configptr = strstr(configstr,"tel.polaralt");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&polaralt);      
        fprintf(stderr,"Polar altitude: %lf\n",polaralt);
      }  
    } 

    configptr = strstr(configstr,"tel.offsetha");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&offsetha);
        offsetha_default = offsetha;
        fprintf(stderr,"Offset in hour angle: %lf\n",offsetha);
      }
    }
    
    configptr = strstr(configstr,"tel.offsetdec");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&offsetdec);
        offsetdec_default = offsetdec;      
        fprintf(stderr,"Offset in declination: %lf\n",offsetdec);
      }
    } 

    configptr = strstr(configstr,"tel.modelha1");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&modelha1_default);
        fprintf(stderr,"Default model parameter ha1: %lf\n",modelha1_default);
        modelha0 = (LSTNow() - telra);
        modelha1 = modelha1_default;
      }
    }
        
    configptr = strstr(configstr,"tel.modeldec1");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&modeldec1_default);
        fprintf(stderr,"Default model parameter dec1: %lf\n",modeldec1_default);
        modeldec0 = teldec;
        modelha0 = (LSTNow() - telra);
        modeldec1 = modeldec1_default;      
      }
    } 
    

    configptr = strstr(configstr,"site.longitude");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&SiteLongitude);
        fprintf(stderr,"Site longitude: %lf\n",SiteLongitude);
      }
    } 
    
    configptr = strstr(configstr,"site.latitude");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&SiteLatitude);
        fprintf(stderr,"Site latitude: %lf\n",SiteLatitude);
      }
    }

    configptr = strstr(configstr,"site.altitude");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&SiteAltitude);
        fprintf(stderr,"Site altitude: %lf\n",SiteAltitude);
      }
    } 
    
    configptr = strstr(configstr,"site.pressure");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&SitePressure);
        fprintf(stderr,"Site pressure: %lf\n",SitePressure);
      }
    } 
    
    configptr = strstr(configstr,"site.temperature");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&SiteTemperature);
        fprintf(stderr,"Site temperature: %lf\n",SiteTemperature);
      }
    }
    
    configptr = strstr(configstr,"ccd.arcsecperpix");
    if ( configptr != NULL)
    {
      configptr = strstr(configstr,"=");
      if ( configptr != NULL)
      {
        configptr = configptr + 1;
        sscanf(configptr,"%lf",&arcsecperpix);
        fprintf(stderr,"CCD image scale arcsec/pixel: %lf\n",arcsecperpix);
      }
    }
    
     
  }
  fclose(fp_config);
}


/* Write ra and dec to a system status file */

void write_coords (double ra, double dec)
{
  FILE* outfile;
  outfile = fopen("/usr/local/observatory/status/telcoords","w");
  if ( outfile == NULL )
  {
    fprintf(stderr,"Cannot update telcoords status file\n");
    return;
  }

  fprintf(outfile, "%lf %lf\n", ra, dec);      
  fclose(outfile);
}





