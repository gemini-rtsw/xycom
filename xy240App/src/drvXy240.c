/* drvXy240.c,v 1.3 2011/09/09 17:56:50 anj Exp */
/***********************************************************************
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory, and the Regents of the University of
 * California, as Operator of Los Alamos National Laboratory.
 * xycom is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 ***********************************************************************/
/*
 *   routines used to test and interface with Xycom240
 *   digital i/o module
 *
 *    Author:      Bob Dalesio
 *    Date:        11/20/91
 * Modification Log:
 * -----------------
 * .01   06-25-92   bg   Added driver to code.  Added xy240_io_report
 *            to it. Added copyright disclaimer.
 * .02   08-10-92   joh   merged xy240_driver.h into this source
 * .03   08-11-92   joh   fixed use of XY240 where XY240_BI or XY240_BO
 *            should have been used
 * .04  08-11-92   joh   now allows for runtime reconfiguration of
 *            the addr map
 * .05  08-25-92        mrk     added DSET; made masks a macro
 * .06  08-26-92        mrk     support epics I/O event scan
 * .07   08-26-92   joh    task params from task params header
 * .08   08-26-92   joh    removed STDIO task option   
 * .09   08-26-92   joh    increased stack size for V5
 * .10   08-26-92   joh    increased stack size for V5
 * .11   08-27-92   joh   fixed no status return from bo driver
 * .12   09-03-92   joh   fixed wrong index used when testing for card
 *            present 
 * .13   09-03-92   joh   fixed structural problems in the io
 *            report routines which caused messages to
 *            be printed even when no xy240's are present 
 * .14   09-17-92   joh   io report now tabs over detailed info
 * .15   09-18-92   joh   documentation
 * .16   08-02-93   mrk   Added call to taskwdInsert
 * .17   08-04-93   mgb   Removed V5/V4 and EPICS_V2 conditionals
 * .18   20160404   mdw   Converted to OSI compliance, simplified xy240_b?_io_report() routines,
 *                        fixed faulty implementation of xy240_dio_out()
 *       20171210   mdw   Begin additions to driver for general support with SCS partuclarly in mind
 *       10171210   mdw   devRegisterAddress [in xy240_init()] was reserving address space for only 1 card 
 */


/* EPICS stuff */
#include <drvSup.h>
#include <devLib.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <taskwd.h>
#include <iocsh.h>
#include <dbAccess.h>
#include <cantProceed.h>
#include <epicsTypes.h>
#include <epicsThread.h>
#include <epicsStdio.h>
#include <epicsExport.h>
#include <epicsPrint.h>
#include <epicsTypes.h>
#include <epicsInterrupt.h>
#include <devSup.h>

#include "drvXy240.h"

/* Number of columns used in io_report. */
#define IOR_MAX_COLS 4

/* default values if the startup script doesn't call drvXy240Config() */
static short xy240_num_cards = 2;
static short xy240_num_channels = 32;
static size_t xy240_addrs = 0x8000;
static short xy240_int_vec = 80;               
static short xy240_int_level = 5;
static size_t xy240_p45_initvalue = 0x0000;
static size_t xy240_p67_initvalue = 0x0000;

#define XY240_ADDR0       xy240_addrs
#define XY240_MAX_CARDS   xy240_num_cards
#define XY240_MAX_CHANS   xy240_num_channels
#define XY240_INT_VEC     xy240_int_vec
#define XY240_INT_LVL     xy240_int_level
#define XY240_P45_IVAL     xy240_p45_initvalue
#define XY240_P67_IVAL     xy240_p67_initvalue

#define XY240_ANY_IRQ     8



#define masks(K) ((1<<K))

/*xy240 control registers structure*/
typedef struct {
   char            vme_id[0x40];           /* VME Module ID in odd-numbered bytes */
   char            begin_pad[0x40];        /* undefined space */
   epicsUInt8      iir;                    /* interrupt inputs register */
   epicsUInt8      csr;                    /* control & status register*/
   epicsUInt8      ipr;                    /* interrupt pending register */
   epicsUInt8      imr;                    /* interrupt mask register */
   epicsUInt8      icr;                    /* interrupt clear register*/
   epicsUInt8      ivr;                    /* interrupt vector register*/
   epicsUInt8      flags;                  /* flag outputs register */
   epicsUInt8      pdr;                    /* port direction register */
   unsigned short  port0_1;                /* port0&1 16 bits value*/
   unsigned short  port2_3;                /* por2&3 16 bits value*/
   unsigned short  port4_5;                /* port4&5 16 bits value*/
   unsigned short  port6_7;                /* port6&7 16 bits value*/
   char            end_pad[0x370];         /* reserved space*/
} volatile xy240Regs_t;

/*create dio control structure record*/
typedef struct {
   xy240Regs_t volatile *dptr;             /* pointer to registers*/
   short num;                              /* device number*/
   short mode;                             /* operating mode*/
   unsigned short sport0_1;                /* saved inputs*/
   unsigned short sport2_3;                /* saved inputs*/
   unsigned short sport4_5;                /* saved inputs*/
   unsigned short sport6_7;                /* saved inputs*/
   IOSCANPVT ioscanpvt;
   epicsUInt8 int_lvl;                     /* interrupt level */
   unsigned int intr_num;                  /* interrupt count*/
   void (*pISR[9])(int);                   /* interrupt service routine pointers */
   epicsBoolean intConnected;              /* true if this card has interrupts connected */
} dio_rec_t;


LOCAL  dio_rec_t *dio = NULL;   /* pointer to array of control structures*/


struct {
   long    number;
   DRVSUPFUN       report;
   DRVSUPFUN       init;
} drvXy240={
   2,
   xy240_io_report,
   xy240_init};
epicsExportAddress(drvet,drvXy240);


/*
 *
 *dio_scan
 *
 *task to check for change of state
 *
 */
/* static int dio_scan() */


static int dio_scan()

{   
   int i;
   int first_scan,first_scan_complete;

   for(;;){
      if(interruptAccept) break;
      epicsThreadSleep(0.05);
   }
   first_scan_complete = FALSE;
   first_scan = TRUE;
   for (;;) {
      for (i = 0; i < XY240_MAX_CARDS; i++) {
            if (dio[i].dptr)
               if (((dio[i].dptr->port0_1) ^ (dio[i].sport0_1)) 
                     || ((dio[i].dptr->port2_3) ^ (dio[i].sport2_3))
                     || first_scan) 
               {
                  /* epicsPrintf("io_scanner_wakeup for card no %d\n",i); */
                  scanIoRequest(dio[i].ioscanpvt);
                  dio[i].sport0_1 = dio[i].dptr->port0_1;
                  dio[i].sport2_3 = dio[i].dptr->port2_3;     
               }
         }
         if (first_scan) {
            first_scan = 0;
            first_scan_complete = 1;
         }
      epicsThreadSleep(0.05);
   }

   return 0;
}


/*DIO DRIVER INIT
 *
 *initialize xy240 dig i/o card
 */
long xy240_init()
{
   short                  val;
   register short         i, j;
   xy240Regs_t           *pdio_xy240;
   int                    status;
   int                    at_least_one_present = FALSE;

   /*Initialize onece only*/
   if (dio != NULL) return OK;  

   printf("xy240_init\n");

   /*
    * allow for runtime reconfiguration of the
    * addr map
    */

   /* to do: revise so that each card can be registered independently and non-contiguously */
   dio = (dio_rec_t *) callocMustSucceed(XY240_MAX_CARDS, sizeof(dio_rec_t), "XY240: Can't allocate memory");

   status = devRegisterAddress("xy240", atVMEA16, XY240_ADDR0, 
                                XY240_MAX_CARDS*sizeof(xy240Regs_t),
                                (void *)&pdio_xy240);
   printf("DBG1 Port 4-5 values: %0#4x, Port 6-7 values: %0#4x\n", pdio_xy240->port4_5, pdio_xy240->port6_7);

   if (status != OK){
      errlogPrintf("%s: Unable to map the XY240 A16 base addr\n", __FILE__);
      return ERROR;
   }
      dio[i].sport6_7 = pdio_xy240->port6_7;   /* read and save high values for Output Port6-7 IA:20170316 */

   errlogPrintf("The value of pdio_xy240 is %p\n", pdio_xy240);
   for (i = 0; i < XY240_MAX_CARDS; i++, pdio_xy240++){

      if(devReadProbe(sizeof(short), pdio_xy240, &val) != OK) {
         errlogPrintf("The value of pdio_xy240 is %p\n", pdio_xy240);
         dio[i].dptr = 0;
         errlogPrintf("XYCOM240 bus error during read cycle.\n");
         continue;
      } 

      /*
       *    register initialization
       */
      
      pdio_xy240->csr = 0x3;                   /* red led off, green led on, interrupts disabled */
      /*pdio_xy240->port4_5 = 0xaaff;   [> this initialises port 4 & 5 to the specified values <]*/
      printf("DBG2 Port 4-5 values: %04x, Port 6-7 values: %04x\n", pdio_xy240->port4_5, pdio_xy240->port6_7);
      pdio_xy240->icr = 0x00;                  /* clear interrupt input register latch */
      
      /* 
       * 20182003 IA Initialises output ports only after a power cycle 
       */
      if (pdio_xy240->port4_5 == 0xffff && pdio_xy240->port6_7 == 0xffff){
          pdio_xy240->port4_5 = XY240_P45_IVAL;   /* this initialises port 4 & 5 to the specified values */
          pdio_xy240->port6_7 = XY240_P67_IVAL;   /* this initialises port 6 & 7 to the specified values */
      }

      pdio_xy240->pdr = 0xf0;                  /* ports 0-3,input;ports 4-7,output */
      printf("DBG3 Port 4-5 values: %04x, Port 6-7 values: %04x\n", pdio_xy240->port4_5, pdio_xy240->port6_7);
      dio[i].sport2_3 = pdio_xy240->port2_3;   /* read and save high values (mdw wants to know why?) */
      printf("DBG4 Port 4-5 values: %0#4x, Port 6-7 values: %#04x\n", pdio_xy240->port4_5, pdio_xy240->port6_7);
      dio[i].dptr = pdio_xy240;                /* store pointer to card registers */

      dio[i].dptr->ivr = XY240_INT_VEC + i;    /* set interrupt vector for this card */
      dio[i].int_lvl   = XY240_INT_LVL;        /* interrupt level must match dip switch setting */

      for(j=0; j<9; ++j)          /* make sure interrupt service routine pointers */
         dio[i].pISR[j] = NULL;   /* are cleared out */

      at_least_one_present = TRUE;
      scanIoInit(&dio[i].ioscanpvt);
   }

   if (at_least_one_present) {

      if ( !epicsThreadCreate("dio_scan", epicsThreadPriorityMedium,
               epicsThreadGetStackSize(epicsThreadStackMedium),
               (EPICSTHREADFUNC)dio_scan, 0) )
      {
         errlogPrintf("Can't start xy240_scan");
         return ERROR;
      }

   }

   return OK;
}


long xy240_getioscanpvt(short card, IOSCANPVT *scanpvt)
{
   if (!(card >= XY240_MAX_CARDS) || (!dio[card].dptr)) 
      *scanpvt = dio[card].ioscanpvt;
   return(0);
}


/*
 * XY240_BI_DRIVER
 *
 *interface to binary inputs
 */

long xy240_bi_driver(short card, epicsUInt32 mask, epicsUInt32 *prval)
{
   register unsigned int   work;

   if ((card >= XY240_MAX_CARDS) || (!dio[card].dptr)) 
      return -1;
   work = (dio[card].dptr->port0_1 << 16) + dio[card].dptr->port2_3;
   *prval = work & mask;

   return(0);
}


/*
 *
 *XY240_BO_READ
 *
 *interface to binary outputs
 */

long xy240_bo_read(short card, epicsUInt32 mask, epicsUInt32 *prval)
{
   epicsUInt32   work;

   if ((card >= XY240_MAX_CARDS) || (!dio[card].dptr)){ 
      return -1;
   }

   work = (dio[card].dptr->port4_5 << 16) + dio[card].dptr->port6_7;

   *prval = (work & mask);

   return(0);
}

/* XY240_DRIVER
 *
 *interface to binary outputs
 */

long xy240_bo_driver(short card, epicsUInt32 val, epicsUInt32 mask)
{
   epicsUInt32   work;

   if ((card >= XY240_MAX_CARDS) || (!dio[card].dptr)) 
      return ERROR;

   /* use structure to handle high and low short swap */
   /* get current output */

   work = (dio[card].dptr->port4_5 << 16)
      + dio[card].dptr->port6_7;

   work = (work & ~mask) | (val & mask);

   dio[card].dptr->port4_5 = (unsigned short)(work >> 16);
   dio[card].dptr->port6_7 = (unsigned short)work;

   return OK;
}


/*dio_out
 *
 *test routine for xy240 output 
 */
int xy240_dio_out(short card, short port, unsigned short val)
{
   unsigned short work[2];

   if ((card > XY240_MAX_CARDS-1)) {      /*test to see if card# is allowable*/
      epicsPrintf("card # out of range\n");
      return -1;
   }

   if (!dio[card].dptr) {            /*see if card exists*/
      epicsPrintf("can't find card %d\n", card);
      return -2;
   }

   if ((port >7) || (port <4)) {     /*make sure they're output ports*/
      epicsPrintf("use ports 4-7\n");
      return -3;
   }


   /* writing to port 4 would clobber port 5 and vice-versa.
    * Likewise with ports 6 and 7. Fixed (mdw) 20160404 */ 
   work[0] = dio[card].dptr->port4_5;
   work[1] = dio[card].dptr->port6_7;

   if (port == 4) {
      dio[card].dptr->port4_5 = ((work[0] & 0xff) | (val<<8)); 
   }
   else if (port == 5) {
      dio[card].dptr->port4_5 = ((work[0] & 0xff00) | val);
   }
   else if (port == 6) {
      dio[card].dptr->port6_7 = ((work[1] & 0xff) | (val<<8)); 
   }
   else if (port == 7) {
      dio[card].dptr->port6_7 = ((work[1] & 0xff00) | val) ;
   }
   epicsPrintf("Set card %d, port %d = 0x%02x\n", card, port, val);
   epicsPrintf("Outputs now: port4=0x%02x, port5=0x%02x, port6=0x%02x, port7=0x%02x\n",
                    (dio[card].dptr->port4_5 >> 8) & 0xff, 
                    dio[card].dptr->port4_5 & 0xff, 
                    (dio[card].dptr->port6_7 >> 8) & 0xff, 
                    dio[card].dptr->port6_7 & 0xff); 
   return -port;
}

/*XY240_WRITE
 *
 *command line interface to test bo driver
 *
 */
int xy240_write(short card, epicsUInt32 val)
{
   return xy240_bo_driver(card,val,0xffffffff);
}



void xy240_bi_io_report(int card)
{
   epicsUInt32 chan, num_chans, val;

   num_chans = XY240_MAX_CHANS;

   epicsPrintf("\tXY240 BINARY IN CHANNELS:\n");
   for(chan=0; chan<num_chans; ++chan) {
         if(xy240_bi_driver(card,masks(chan),&val) == 0)
            epicsPrintf("\tChan %d = %x\t ",chan, val?1:0);
         else { 
            epicsPrintf("Invalid Card\n");
            return;
         }
         if((chan%IOR_MAX_COLS) == (IOR_MAX_COLS-1)) epicsPrintf("\n");
   }
}

   
void xy240_bo_io_report(int card)
{
   epicsUInt32 chan, num_chans, val;

   num_chans = XY240_MAX_CHANS;

   epicsPrintf("\tXY240 BINARY OUT CHANNELS:\n");

   for(chan=0; chan<num_chans; ++chan) {
      if(xy240_bo_read(card,masks(chan),&val) == 0)
         epicsPrintf("\tChan %d = %x\t ", chan, val?1:0);
      else 
      {
         epicsPrintf("Invalid Card\n");
         return;
      }
      if((chan%IOR_MAX_COLS) == (IOR_MAX_COLS-1)) epicsPrintf("\n");
   }
}


long xy240_io_report(int level)
{
   int card;

   for (card = 0; card < XY240_MAX_CARDS; card++){

      if(dio[card].dptr){
         epicsPrintf("B*: XY240:\tcard %d\n",card);
         if (level >= 1){
            xy240_bi_io_report(card);
            xy240_bo_io_report(card);
         }
      }
   }
   return(0); 
}


int drvXy240Config(unsigned int ncards, unsigned int nchannels, size_t base)
{
   xy240_num_cards = ncards;
   xy240_num_channels = nchannels;
   xy240_addrs = base;
   
   /*Don't do this matt*/
  xy240_init();
   return 0;
}

/*IA 20181903: This function allows the output ports to be initilised to a desired value*/
int drvXy240ConfigOutputInit(
        unsigned int ncards,
        unsigned int nchannels,
        size_t base,
        size_t p45init,
        size_t p67init )
{
   xy240_p45_initvalue = p45init;
   xy240_p67_initvalue = p67init;
   xy240_num_cards = ncards;
   xy240_num_channels = nchannels;
   xy240_addrs = base;
   
   /*Don't do this matt*/
  xy240_init();
   return 0;
}

int drvXy240ConfigWithInterrupts(
             unsigned int ncards, 
             unsigned int nchannels, 
             size_t base, 
             unsigned int int_vec, 
             unsigned int int_level )
{
   drvXy240Config(ncards, nchannels, base);
   xy240_int_vec = int_vec;
   xy240_int_level = int_level;
   
   return 0;
}


static const iocshArg drvXy240ConfigArg0 = { "number of cards",            iocshArgInt };
static const iocshArg drvXy240ConfigArg1 = { "number of channel per card", iocshArgInt };
static const iocshArg drvXy240ConfigArg2 = { "base address of first card", iocshArgInt };
static const iocshArg drvXy240ConfigArg3 = { "interrupt vector",           iocshArgInt };
static const iocshArg drvXy240ConfigArg4 = { "interrupt level",            iocshArgInt };

static const iocshArg *drvXy240ConfigArgs[] = { 
   &drvXy240ConfigArg0, 
   &drvXy240ConfigArg1, 
   &drvXy240ConfigArg2,
   &drvXy240ConfigArg3,
   &drvXy240ConfigArg4 
};

static const iocshArg drvXy240ConfigOutputInitArg0 = { "number of cards",            iocshArgInt };
static const iocshArg drvXy240ConfigOutputInitArg1 = { "number of channel per card", iocshArgInt };
static const iocshArg drvXy240ConfigOutputInitArg2 = { "base address of first card", iocshArgInt };
static const iocshArg drvXy240ConfigOutputInitArg3 = { "Ports 4 & 5 initial value",  iocshArgInt };
static const iocshArg drvXy240ConfigOutputInitArg4 = { "Ports 6 & 7 initial value",  iocshArgInt };

static const iocshArg *drvXy240ConfigOutputInitArgs[] = { 
   &drvXy240ConfigOutputInitArg0, 
   &drvXy240ConfigOutputInitArg1, 
   &drvXy240ConfigOutputInitArg2,
   &drvXy240ConfigOutputInitArg3,
   &drvXy240ConfigOutputInitArg4 
};

static const iocshFuncDef drvXy240ConfigFuncDef =
         {"drvXy240Config", 3, drvXy240ConfigArgs}; 
static void drvXy240ConfigCallFunc(const iocshArgBuf *args )
{
         drvXy240Config(args[0].ival, args[1].ival, args[2].ival);
}

static const iocshFuncDef drvXy240ConfigWithInterruptsFuncDef =
      {"drvXy240ConfigWithInterrupts", 5, drvXy240ConfigArgs}; 
static void drvXy240ConfigWithInterruptsCallFunc(const iocshArgBuf *args )
{
   drvXy240ConfigWithInterrupts(args[0].ival, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
}

static const iocshFuncDef drvXy240ConfigOutputInitFuncDef =
      {"drvXy240ConfigOutputInit", 5, drvXy240ConfigOutputInitArgs}; 
static void drvXy240ConfigOutputInitCallFunc(const iocshArgBuf *args )
{
   drvXy240ConfigOutputInit(args[0].ival, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
}


static const iocshArg xy240_cardArg  = { "card number",   iocshArgInt };
static const iocshArg xy240_portArg  = { "port number",   iocshArgInt };
static const iocshArg xy240_boolArg  = { "boolean value", iocshArgInt };
static const iocshArg xy240_ledArg   = { "LED (red=0, green=1)", iocshArgInt };
static const iocshArg xy240_val8Arg  = { "8-bit value",   iocshArgInt };
static const iocshArg xy240_val16Arg = { "16-bit value",  iocshArgInt };
static const iocshArg xy240_val32Arg = { "32-bit value",  iocshArgInt };
static const iocshArg xy240_val64Arg = { "64-bit value",  iocshArgInt };
static const iocshArg xy240_bitArg   = { "bit number",    iocshArgInt };

static const iocshArg *xy240_dio_outArgs[] = { 
      &xy240_cardArg, 
      &xy240_portArg, 
      &xy240_val8Arg
 };
static const iocshFuncDef xy240_dio_outFuncDef =
   {"xy240_dio_out", 3, xy240_dio_outArgs}; 
static void xy240_dio_outCallFunc(const iocshArgBuf *args )
{
   xy240_dio_out(args[0].ival, args[1].ival, args[2].ival);
}

static const iocshArg *xy240_writeArgs[] = { 
      &xy240_cardArg, 
      &xy240_val32Arg 
};
static const iocshFuncDef xy240_writeFuncDef =
   {"xy240_write", 2, xy240_writeArgs}; 
static void xy240_writeCallFunc(const iocshArgBuf *args )
{
   xy240_write(args[0].ival, args[1].ival);
}



void iocshXy240WritePortBit(int card, int port, int bit, int val)
{
      epicsPrintf("XY240: Setting bit %d of port %d on card %d to %d\n",
                 bit, port, card, val);
      xy240_writePortBit(card, port, bit, val);
}
static const iocshArg *xy240_writePortBitArgs[] = {
      &xy240_cardArg,
      &xy240_portArg,
      &xy240_bitArg,
      &xy240_boolArg
};
static const iocshFuncDef xy240_writePortBitFuncDef =
   {"xy240_writePortBit", 4, xy240_writePortBitArgs}; 
static void xy240_writePortBitCallFunc(const iocshArgBuf *args )
{
   iocshXy240WritePortBit(args[0].ival, args[1].ival, args[2].ival, args[3].ival);
}
      


void iocshXy240WritePortByte(int card, int port, epicsUInt8 byte)
{
      epicsPrintf("XY240: Writing 0x%02X to port %d on card %d\n",
               byte, port, card);
      xy240_writePortByte(card, port, byte);
}
static const iocshArg *xy240_writePortByteArgs[] = {
      &xy240_cardArg,
      &xy240_portArg,
      &xy240_val8Arg
};
static const iocshFuncDef xy240_writePortByteFuncDef =
   {"xy240_writePortByte", 3, xy240_writePortByteArgs}; 
static void xy240_writePortByteCallFunc(const iocshArgBuf *args )
{
   iocshXy240WritePortByte(args[0].ival, args[1].ival, args[2].ival);
}
      

void iocshXy240ReadPortBit(int card, int port, int bit)
{
   epicsPrintf("XY240: bit %d of port %d on card %d = %d\n",
            bit, port, card, xy240_readPortBit(card, port, bit));
}
static const iocshArg *xy240_readPortBitArgs[] = {
      &xy240_cardArg,
      &xy240_portArg,
      &xy240_bitArg,
};
static const iocshFuncDef xy240_readPortBitFuncDef =
   {"xy240_readPortBit", 3, xy240_readPortBitArgs}; 
static void xy240_readPortBitCallFunc(const iocshArgBuf *args )
{
   iocshXy240ReadPortBit(args[0].ival, args[1].ival, args[2].ival);
}
      



void iocshXy240ReadPortByte(int card, int port)
{
   epicsPrintf("XY240: port %d on card %d = 0x%02X\n",
            port, card, xy240_readPortByte(card, port));
}
static const iocshArg *xy240_readPortByteArgs[] = {
      &xy240_cardArg,
      &xy240_portArg,
};
static const iocshFuncDef xy240_readPortByteFuncDef =
   {"xy240_readPortByte", 2, xy240_readPortByteArgs}; 
static void xy240_readPortByteCallFunc(const iocshArgBuf *args )
{
   iocshXy240ReadPortByte(args[0].ival, args[1].ival);
}
      


void iocshXy240LedCtl(int card, int led, int val)
{
      epicsPrintf("XY240: Turning the %s LED %s for card %d\n",
         led?"green":"red", val?"on":"off", card);
      if(!led) val=!val;  /* red led is inverse */
      xy240_ledCtl(card, led, val); 
}
static const iocshArg *xy240_ledCtlArgs[] = {
      &xy240_cardArg,
      &xy240_ledArg,
      &xy240_boolArg
};
static const iocshFuncDef xy240_ledCtlFuncDef =
   {"xy240_ledCtl", 3, xy240_ledCtlArgs}; 
static void xy240_ledCtlCallFunc(const iocshArgBuf *args )
{
   iocshXy240LedCtl(args[0].ival, args[1].ival, args[2].ival);
}
      


//static const xy240_writeIMRArgs[] = {} //(int cardnum, epicsUInt8 byteval);
//static const xy240_readIPRArgs[] = {}  //(int cardnum);
//static const xy240_writeFlagBitArgs[] = {} // (int cardnum, epicsUInt8 bitnum, epicsBoolean bitval);
//static const xy240_setResetFlagBitsArgs[] = {} //(int cardnum, epicsUInt8 bitmask, epicsBoolean set);
//static const xy240_writeFlagByteArgs[] = {} // (int cardnum, epicsUInt8 bitnum, epicsUInt8 byteval);
//static const xy240_writeCSRBitArgs[] = {} //(int cardnum, epicsUInt8 bitnum, epicsBoolean bitval);

static void drvXy240RegisterCommands(void)
{
   static int firstTime = 1;
   if (firstTime) {
      iocshRegister(&drvXy240ConfigFuncDef, drvXy240ConfigCallFunc);
      iocshRegister(&drvXy240ConfigOutputInitFuncDef, drvXy240ConfigOutputInitCallFunc);
      iocshRegister(&drvXy240ConfigWithInterruptsFuncDef, drvXy240ConfigWithInterruptsCallFunc);
      iocshRegister(&xy240_dio_outFuncDef, xy240_dio_outCallFunc);
      iocshRegister(&xy240_writeFuncDef, xy240_writeCallFunc);
      iocshRegister(&xy240_writePortBitFuncDef, xy240_writePortBitCallFunc);
      iocshRegister(&xy240_writePortByteFuncDef, xy240_writePortByteCallFunc);
      iocshRegister(&xy240_readPortBitFuncDef, xy240_readPortBitCallFunc);
      iocshRegister(&xy240_readPortByteFuncDef, xy240_readPortByteCallFunc);
      iocshRegister(&xy240_ledCtlFuncDef, xy240_ledCtlCallFunc);
      firstTime = 0;
   }
}
epicsExportRegistrar(drvXy240RegisterCommands);




/********************************************************************************************
 * This stuff added to support SCS                                                          *
 * 07-Dec-2017 (mdw)
 * ******************************************************************************************/

/*******************************************************************************************
 * xy240_status()
 * return the initialization status of the xy240 card                                      *
 *******************************************************************************************/
unsigned long xy240_status(int cardnum) 
{

   if(cardnum<0 || cardnum>XY240_MAX_CARDS || dio[cardnum].dptr==NULL)
      return S_dev_NoInit;

   return OK;
}

/* some diagnostic variables */
int xy240_isr_bogus_card_cnt = 0;
epicsExportAddress(int, xy240_isr_bogus_card_cnt);

int xy240_isr_bogus_int_cnt = 0;
epicsExportAddress(int, xy240_isr_bogus_int_cnt);

int xy240_isr_int_not_pending_cnt = 0;
epicsExportAddress(int, xy240_isr_int_not_pending_cnt);

void xy240_isr(void *p) /*  p is number of the card that caused the interrupt */
{
   int key = epicsInterruptLock();
   int cardnum = (int)(long)p;
   register int i;

   /* make sure card number isn't bogus */
   if (dio[cardnum].dptr == NULL || cardnum < 0 || cardnum >= XY240_MAX_CARDS) {
      xy240_isr_bogus_card_cnt++;
      epicsInterruptUnlock(key);
      return;
   }
   
   /* make sure card has been set to use interrupts */
   if(!dio[cardnum].intConnected) {
      dio[cardnum].dptr->csr &= ~XY240_CSR_INT_ENA;   /* disable interrupts on card */
      xy240_isr_bogus_int_cnt++;
      epicsInterruptUnlock(key);
      return;
   }
    
   /* figure out if there is actually an interrupt pending */
   if(!(dio[cardnum].dptr->csr & XY240_CSR_INT_PENDING)) {
     dio[cardnum].dptr->icr = 0xff; /* clear all interrupts */
     xy240_isr_int_not_pending_cnt++;
     epicsInterruptUnlock(key);
     return;
   }  

   /* now execute the connected handler function */

   /* if we've hooked a routine to run on any interrupt */ 
   if(dio[cardnum].pISR[XY240_ANY_IRQ])
      (*dio[cardnum].pISR[XY240_ANY_IRQ])(cardnum);

   /* now run routines for individual irq channels */
   for (i=0; i<8; ++i) 
      if(dio[cardnum].dptr->ipr & masks(i))
         if(dio[cardnum].pISR[i] != NULL) /* check for null pointer to handler */
            (*dio[cardnum].pISR[i])(cardnum); // call handler with card number

   /* clear all interrupts */            
   dio[cardnum].dptr->icr = 0xff; /* clear all interrupts */

   epicsInterruptUnlock(key);
}


/*****************************************************************************
*
* xy240_intConnect - Connect a C routine up to an XY240 Interrupt
*
* This is used to register an interrupt routine with the xy240  driver layer,
* and initialises the registers on the xy240 card to enable interrupts
* for the given RM interrupt channel number.
*
* The interrupt routine should expect a single integer parameter, which 
* will contain the card number of the xy240 interrupt source.
*
*
* RETURNS:
* OK, or S_dev_??? errors.
* 
* EXAMPLE:
* .CS
*   void myIsr(int cardnum);
*   int status;
*
*   status = xy240_intConnect(cardnum, irqchan, myIsr);
*   if(status != 0)
*       errlogPrintf("Can't connect xy240 interrupt for irq %d on card %d. status=%d\n", 
*                     irqchan, cardnum, status, );
* .CE
*
* SEE ALSO: xy240IntDisconnect()
*/
long xy240_intConnect(int cardnum, int irqchan, void (*proutine)(int)) 
{
   long status;

   if (dio[cardnum].dptr == NULL)
      return S_dev_NoInit;
   if(irqchan<0 || irqchan>XY240_ANY_IRQ)
      return S_dev_vecInstlFail;
   if(dio[cardnum].pISR[irqchan] != NULL)
      return S_dev_vectorInUse;

   /* connect the card's interrupt if not already connected */
   if(!dio[cardnum].intConnected) {
      if(devInterruptInUseVME(dio[cardnum].dptr->ivr))
         return S_dev_vectorInUse;
      status = devConnectInterruptVME(dio[cardnum].dptr->ivr, xy240_isr, (void *)(long)cardnum);
      if (status != OK)
         return status;
      dio[cardnum].dptr->csr |= XY240_CSR_INT_ENA;               /* enable card's master interrupt control */ 
      status = devEnableInterruptLevelVME(dio[cardnum].int_lvl); /* enable system interrupt level */
      if (status != OK)
         return status;
      dio[cardnum].intConnected = epicsTrue;                     /* indicate that this card is interrupting */
   }

   /* save the routine pointer */
   dio[cardnum].pISR[irqchan] = proutine;
   
   /* now enable the interrupt on the hardware */
   dio[cardnum].dptr->icr = 0xFF;           /* clear interrupts */
   /* a specific call to xy240_writeIMR() is needed in the case that irqchan=XY240_ANY_IRQ */
   if(irqchan<XY240_ANY_IRQ)
       dio[cardnum].dptr->imr |= (1 << irqchan); /* unmask this interrupt source */

   return OK;
}

/*****************************************************************************
*
* xy240_intDisconnect - Disconnect an xy240 interrupt routine
*
* This disables Xycom-240 interrupts on the given card and channel and marks the
* channel as unused.
*
* RETURNS:
* OK, or S_dev_???
*
* EXAMPLE:
* .CS
*   int status;
*
*   status = xy240_intDisconnect(icardnum, irqchan);
*   if (status)
*       printf("No ISR Connected\\n");
* .CE
*
* SEE ALSO: xy240_intConnect()
*/
long xy240_intDisconnect(int cardnum, int irqchan) 
{
   register int i;
   if(cardnum<0 || cardnum>=XY240_MAX_CARDS) 
      return S_dev_NoInit;

   if(irqchan<0 || irqchan>XY240_ANY_IRQ || !dio[cardnum].pISR[irqchan])
      return S_dev_vectorNotInUse;

   /* Mask this irq channel on the hardware. In the case of irqchan==XY240_ANY_IRQ,
    * the calling program will also have to call xy240_writeIMR() first to mask the channels
    * that it was interested in */ 
   if(irqchan<XY240_ANY_IRQ)
     dio[cardnum].dptr->imr &= !masks(irqchan); 

   /* clear the interrupt routine pointer for this irq channel */
   dio[cardnum].pISR[irqchan] = NULL;

   /* if there are any routines left connected, don't disconnect this card's main ISR */
   for(i=0; i<=XY240_ANY_IRQ; i++)
      if(dio[cardnum].pISR[i])
         return OK;

   /* disable the card's interrupt */ 
   dio[cardnum].dptr->csr &= ~XY240_CSR_INT_ENA;

   /* indicate that this card is not interrupting */
   dio[cardnum].intConnected = epicsFalse;

   /* disconnect the main ISR */
   return devDisconnectInterruptVME(dio[cardnum].dptr->ivr, xy240_isr);

}


/* get the contents of the Interrupts Pending Register */
int  xy240_readIPR(int cardnum) 
{
   if(cardnum<0 || cardnum>=XY240_MAX_CARDS || !dio[cardnum].dptr)
      return ERROR;

   return dio[cardnum].dptr->ipr;
}




int xy240_writePortBit(int cardnum, epicsUInt8 portnum, epicsUInt8 bitnum, epicsBoolean bitval) 
{
   epicsUInt8 volatile *port;

   if(cardnum<0 || cardnum>=XY240_MAX_CARDS) {
      errlogPrintf("xy240_writePortBit(): bad card number %d\n", cardnum);
      return ERROR;
   }

   if(portnum>7) {
      errlogPrintf("xy240_writePortBit(): bad port number %d for card %d\n",
                     portnum, cardnum);
      return ERROR;
   }
   
   if(bitnum>7) {
      errlogPrintf("xy240_writePortBit(): bad bit number %d for port  %d for card %d\n",
                     bitnum, portnum, cardnum);
      return ERROR;
   }
   
   port  = (epicsUInt8 *)&dio[cardnum].dptr->port0_1;

   if(bitval)
      port[portnum] |= masks(bitnum);
   else
      port[portnum] &= ~masks(bitnum);

   return OK;
}

int xy240_writePortByte(int cardnum, epicsUInt8 portnum, epicsUInt8 byteval) 
{
   epicsUInt8 volatile *port;

   if(cardnum<0 || cardnum>=XY240_MAX_CARDS) {
      errlogPrintf("xy240_writePortByte(): bad card number %d\n", cardnum);
      return ERROR;
   }

   if(portnum>7) {
      errlogPrintf("xy240_writePortByte(): bad port number %d for card %d\n",
                     portnum, cardnum);
      return ERROR;
   }
   
   port  = (epicsUInt8 *)&dio[cardnum].dptr->port0_1;

   port[portnum] = byteval;

   return OK;
}

int xy240_writeFlagBit(int cardnum, epicsUInt8 bitnum, epicsBoolean bitval) 
{
   if(cardnum<0 || cardnum>XY240_MAX_CARDS || !dio[cardnum].dptr ) {
      errlogPrintf("xy240_writeFlagBit(): Bad card number %d\n", cardnum);
      return ERROR;
   }

   if(bitnum<0 || bitnum>7) {
      errlogPrintf("xy240_writeFlagBit(): Bad bit number %d on card %d\n", bitnum, cardnum);
      return ERROR;
   }

   if(bitval)
      dio[cardnum].dptr->flags |= masks(bitnum);
   else
      dio[cardnum].dptr->flags &= ~masks(bitnum);
      
   return OK;
}


int xy240_setResetFlagBits(int cardnum, epicsUInt8 bitmask, epicsBoolean set) 
{
   if(cardnum<0 || cardnum>XY240_MAX_CARDS || !dio[cardnum].dptr) {
      errlogPrintf("xy240_writeFlagBit(): Bad card number %d\n", cardnum);
      return ERROR;
   }


   if(set) /* set bits in specified in mask */
      dio[cardnum].dptr->flags |= bitmask; 
   else    /* clear bits specified in mask */
      dio[cardnum].dptr->flags &= ~bitmask;
      
   return OK;
}

int xy240_writeFlagByte(int cardnum, epicsUInt8 bitnum, epicsUInt8 byteval) 
{
   if(cardnum<0 || cardnum>XY240_MAX_CARDS) {
      errlogPrintf("xy240_writeFlagByte(): Bad card number %d\n", cardnum);
      return ERROR;
   }

   dio[cardnum].dptr->flags = byteval;
      
   return OK;
}

/* Write interrupt mask register */
int xy240_writeIMR(int cardnum, epicsUInt8 byteval) 
{
   if(cardnum<0 || cardnum >=XY240_MAX_CARDS || !dio[cardnum].dptr) {
      errlogPrintf("xy240_writeIMR(): bad card number %d\n", cardnum);
      return ERROR;
   }
   
   dio[cardnum].dptr->imr = byteval;
   return OK;
}


/* Set or reset an individual Control and Status Register bit */
int xy240_writeCSRBit(int cardnum, epicsUInt8 bitnum, epicsBoolean bitval)
{
   if(cardnum<0 || cardnum>XY240_MAX_CARDS || !dio[cardnum].dptr ) {
      errlogPrintf("xy240_writeSCSBit(): Bad card number %d\n", cardnum);
      return ERROR;
   }

   if(bitnum<0 || bitnum>7) {
      errlogPrintf("xy240_writeSCSBit(): Bad bit number %d on card %d\n", bitnum, cardnum);
      return ERROR;
   }

   if(bitval)
      dio[cardnum].dptr->csr |= masks(bitnum);
   else
      dio[cardnum].dptr->csr &= ~masks(bitnum);

   return OK;
}

/* read the value of a port */
epicsInt16 xy240_readPortByte(int cardnum, epicsUInt8 portnum) 
{
   epicsUInt8 volatile *port;

   if(cardnum<0 || cardnum>XY240_MAX_CARDS || !dio[cardnum].dptr ) {
      errlogPrintf("xy240_readByte(): Bad card number %d\n", cardnum);
      return ERROR;
   }

   port = (epicsUInt8 *)&dio[cardnum].dptr->port0_1;

   return port[portnum];
}


/* read the value of a bit on a port */
epicsInt16 xy240_readPortBit(int cardnum, epicsUInt8 portnum, epicsUInt8 bitnum) 
{
   epicsUInt8 volatile *port;

   if(cardnum<0 || cardnum>XY240_MAX_CARDS || !dio[cardnum].dptr ) {
      errlogPrintf("xy240_readByte(): Bad card number %d\n", cardnum);
      return ERROR;
   }

   port = (epicsUInt8 *)&dio[cardnum].dptr->port0_1;

   if(port[portnum] &  masks(bitnum))
      return epicsTrue;
   return epicsFalse;
}

/* control the state of the front panel LEDs */
int xy240_ledCtl(int cardnum, epicsUInt8 led, epicsBoolean val)
{
   if(led>1) {
      errlogPrintf("xy240_ledCtl(): bad LED number %d for card %d\n",
                     led,  cardnum);
      return ERROR;
   }

   if (led) /* green LED */
      xy240_writeCSRBit(cardnum, 1, val);   
   else     /* red LED */
      xy240_writeCSRBit(cardnum, 0, !val); /* red LED is inverse value */   

   return OK;
}








#if 0
/* this stuff not needed for SCS.... Complete later, maybe. */

void * xy240GetBaseAddr(int boardNum) {}

 
int xy240ReadBit(int card, epicsUInt8 port, epicsUInt8  bitnum) {}

int xy240ReadFlagByte(int card) {}
int xy240ReadFlagBit(int card, epicsUInt8 bitnum) {}

int xy240ReadIIR(int card) {}
int xy240ReadIVR(int card) {}
int xy240WriteIVR(int card, epicsUInt8 byteval) {}
int xy240ReadIMR(int card) {}
int xy240ReadIMRBit(int card, epicsUInt8 bitnum) {}
int xy240WriteIMRBit(int card, epicsUInt8 bitnum, epicsBoolean bitval) {}
int xy240WriteICR(int card, epicsUInt8 byteval) {}
int xy240WriteICRBit(int cardnum, epicsUInt8 bitnum, epicsBoolean bitval) {}


int xy240ReadPDR(int card) {}
int xy240ReadPDRBit(int card, epicsUInt8 bitnum) {} 
int xy240WritePDR(int card, epicsUInt8 byteval) {}
int xy240WritePDRBit(int card, epicsUInt8 bitnum, epicsBoolean bitval) {}


int xy240ReadCSR(int card) {}
int xy240ReadCSRBit(int card, epicsUInt8 bitnum) {}
int xy240WriteCSR(int card, epicsUInt8 byteval) {}
int xy240WriteCSRBit(int card, epicsUInt8 bitnum, epicsBoolean bitval) {}


int xy240Help(void) {/* print out available xy240 shell commands */}

#endif
