/* devWfXy566Sc.c */
/* base/src/dev devWfXy566Sc.c,v 1.5 2004/01/21 20:41:23 mrk Exp */

/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory.
* xycom is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/
/* devWfXy566Sc.c - Device Support Routines */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  02-05-92   jba   Changed function arguments from paddr to precord 
 * .03  02-28-92        jba     Changed callback handling, ANSI C changes
 * .04   03-13-92   jba   ANSI C changes
 * .05   04-18-92   jba   removed process from init_record parms
 *      ...
 */

#if 0
#include <vxWorks.h>
#include <stdio.h>
#endif

#include <string.h>

#include <alarm.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <link.h>
#include <waveformRecord.h>
#include <epicsExport.h>

#include "drvXy566.h"


static long init_record();
static long read_wf();
static long arm_wf();


struct {
   long      number;
   DEVSUPFUN   report;
   DEVSUPFUN   init;
   DEVSUPFUN   init_record;
   DEVSUPFUN       get_ioint_info;
   DEVSUPFUN   read_wf;
} devWfXy566Sc={
   5,
   NULL,
   NULL,
   init_record,
   NULL,
   read_wf};
epicsExportAddress(dset,devWfXy566Sc);


static void myCallback(struct waveformRecord *pwf, int no_read, unsigned char *pdata)
{
   struct rset *prset=(struct rset *)(pwf->rset);
   short ftvl = pwf->ftvl;

   if(!pwf->busy) return;
   dbScanLock((struct dbCommon *)pwf);
   pwf->busy = FALSE;
   if(ftvl==DBF_SHORT || ftvl==DBF_USHORT) {
      memcpy(pwf->bptr,pdata,no_read*2);
      pwf->nord = no_read;            /* number of values read */
   } 
   else {
      recGblRecordError(S_db_badField,(void *)pwf, "read_wf - illegal ftvl");
      recGblSetSevr(pwf,READ_ALARM,INVALID_ALARM);
   }
   (*prset->process)(pwf);
   dbScanUnlock((struct dbCommon *)pwf);
}

static long init_record(struct waveformRecord *pwf)
{

   /* wf.inp must be an VME_IO */
   switch (pwf->inp.type) {
      case (VME_IO) :
         break;
      default :
         recGblRecordError(S_db_badField,(void *)pwf, "devWfXy566Sc (init_record) Illegal INP field");
         return(S_db_badField);
   }
   return(0);
}

static long read_wf(struct waveformRecord *pwf)
{
   
   /* determine if wave form is to be rearmed*/
   /* If not active then request rearm */
   if(!pwf->pact) arm_wf(pwf);
   /* if already active then call is from myCallback. check rarm*/
   else if(pwf->rarm) {
      (void)arm_wf(pwf);
      pwf->rarm = 0;
   }
   return(0);
}

static long arm_wf(struct waveformRecord *pwf)
{
   struct vmeio *pvmeio = (struct vmeio *)&(pwf->inp.value);

   pwf->busy = TRUE;
   if(xy566_driver(pvmeio->card,myCallback,pwf)<0) {
      recGblSetSevr(pwf,READ_ALARM,INVALID_ALARM);
      pwf->busy = FALSE;
      return(0);
   }
   pwf->pact=TRUE;
   return(0);
}
