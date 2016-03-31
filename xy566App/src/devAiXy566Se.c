/* devAiXy566Se.c */
/* base/src/dev devAiXy566Se.c,v 1.5 2004/01/21 20:41:23 mrk Exp */

/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory.
* xycom is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/
/* devAiXy566Se.c - Device Support Routines */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02   03-13-92   jba   ANSI C changes
 * .03   02-08-94   mrk   Issue Hardware Errors BUT prevent Error Message Storms
 *      ...
 */
#if 0
#include <stdlib.h>
#include <string.h>
#include <epicsStdioRedirect.h>
#endif

#include <errlog.h>
#include <alarm.h>
#include <cvtTable.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <link.h>
#include <aiRecord.h>
#include <epicsExport.h>

#include "drvXy566.h"


static long xy566Se_init_record();
static long xy566Se_read_ai();
static long xy566Se_special_linconv();

struct {
   long      number;
   DEVSUPFUN   report;
   DEVSUPFUN   init;
   DEVSUPFUN   init_record;
   DEVSUPFUN       get_ioint_info;
   DEVSUPFUN   read_ai;
   DEVSUPFUN   special_linconv;
} devAiXy566Se={
   6,
   NULL,
   NULL,
   xy566Se_init_record,
   NULL,
   xy566Se_read_ai,
   xy566Se_special_linconv};
epicsExportAddress(dset,devAiXy566Se);


static long xy566Se_init_record(struct aiRecord *pai)
{
   unsigned short value;
   struct vmeio *pvmeio;
   long status;

   /* ai.inp must be an VME_IO */
   switch (pai->inp.type) {
      case (VME_IO) :
         break;
      default :
         recGblRecordError(S_db_badField,(void *)pai, "devAiXy566Se (init_record) Illegal INP field");
         return(S_db_badField);
   }

   /* set linear conversion slope*/
   pai->eslo = (pai->eguf -pai->egul)/4095.0;

   /* call driver so that it configures card */
   pvmeio = (struct vmeio *)&(pai->inp.value);
   if((status=ai_xy566_driver(pvmeio->card,pvmeio->signal,XY566SE,&value))) {
      recGblRecordError(status,(void *)pai, "devAiXy566Se (init_record) ai_xy566_driver error");
      return(status);
   }
   return(0);
}

static long xy566Se_read_ai(struct aiRecord *pai)
{
   unsigned short value;
   struct vmeio *pvmeio;
   long status;

   
   pvmeio = (struct vmeio *)&(pai->inp.value);
   status=ai_xy566_driver(pvmeio->card,pvmeio->signal,XY566SE,&value);
   if(status==-1) {
      status = 2; /*don't convert*/
      if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
                       && (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
         recGblRecordError(-1,(void *)pai,"ai_xy566_driver Error");
         return(status);
   } else if(status==-2) {
        status=0;
        recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
     }
   if(status!=0) return(status);
   pai->rval = value & 0xfff;
   return(status);
}

static long xy566Se_special_linconv(struct aiRecord *pai, int after)
{

   if(!after) return(0);
   /* set linear conversion slope*/
   pai->eslo = (pai->eguf -pai->egul)/4095.0;
   return(0);
}
