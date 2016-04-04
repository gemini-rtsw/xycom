/** drvXy566.c,v 1.8 2004/01/21 20:41:23 mrk Exp */
/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory.
* xycom is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/
/* Driver Support Routines for Xycom xy566
 *
 *    Author:      Bob Dalesio
 *    Date:        6-13-88
 *
 * Modification Log:
 * -----------------
 * .00   09-14-88   lrd   check for IGEN card present before initing
 * .01   9-27-88      lrd   removed test code
 * .02   09-27-88   lrd   removed call to xy_mem_init
 * .03   10-04-88   lrd   remove IGEN support
 * .04   10-07-88   lrd   add support for the Xycom 085 arm mechanism
 *            added external latched AI and made
 *            others scan continuously 
 * .05   11-10-88   lrd   change addresses per HW Tech Note #2
 * .06   02-08-89   lrd   moved module addresses into a table in
 *            module_types.h from ai_driver.h
 * .07   02-24-89   lrd   modify for vxWorks 4.0
 *            changed sysSetBCL to sysIntEnable
 * .08   04-17-89   lrd   add callback mechanism for data take complete
 * .09   05-10-89   lrd   increased performance for xycom 566 interface
 *            by keeping the address of the memory buffers
 *            thus removing the need to calculate on each 
 *            read
 * .10   07-27-89   lrd   modified to use channel 0 not channel 1
 * .11   11-20-89   joh   added call to the at5vxi ai driver
 * .12   02-15-90   lrd   modified for new 085 card
 *   02/04/91   ges   Change taskDelay from 6 to 2 in 
 *            "xy566DoneTask". Allows rearm and data reads 
 *            for successive waveform scans up thru 
 *            10 Hz rates.
 * .13   08-27-91    bg   broke the 566 driver out of ai_driver.c
 *                              and moved it to this file. Moved io_report code
 *                              to this file. 
 *                              added arguments and raw value read out 
 *            for analog in cards.
 * .14   10/30/91   bg   Combined the xy566 waveform driver with the
 *                               xy566 analog in drivers. Changed addressing to
 *                               use sysBusToLocalAddrs and VME_AM_STD_SUP or
 *                               VME_AM_SUP_SHORT_IO
 * .15  11-30-91         bg   Added sysBusToLocalAdrs to both ai and 
 *            waveform sections.                  
 * .16  02-05-92         bg   Changed io_report so it has an argument 
 *            level and so if the level > 1, the raw 
 *            value from the card will be printed out 
 *            for analog ins only.
 * .17   03/20/92   bg   Added the argument level to io_report, 
 *            but so far it doesn't do anything. Will 
 *            add ability to report ability to read out 
 *            raw value if there is a demand.
 * .18  08-10-92   joh   cleaned up the merge of the xy566 wf and ai
 *            drivers
 * .19  08-25-92         mrk     replaced call to ai_driver by ai_xy566_driver
 * .20  08-26-92         mrk     support epics I/O event scan
 * .21  08-27-92   joh   fixed routines which return with and without
 *            status   
 * .22  08-27-92   joh   fixed nonexsistant EPICS init 
 * .23  08-02-93   mrk   Added call to taskwdInsert
 * .24  08-04-93   mgb   Removed V5/V4 and EPICS_V2 conditionals
 */


#if 0
/* vxWorks stuff */
#include <vxWorks.h>
#include <stdlib.h>
#include <rebootLib.h>
#include <intLib.h>
#include <iv.h>
#include <vme.h>
#include <sysLib.h>
#include <logLib.h>
#include <vxLib.h>
#endif

/* EPICS stuff */
#include <dbDefs.h>
#include <dbScan.h>
#include <drvSup.h>
#include <devLib.h>
#include <taskwd.h>
#include <errlog.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <epicsStdlib.h>
#include <epicsStdio.h>
#include <epicsExport.h>
#include <epicsExit.h>
#include <epicsInterrupt.h>

#include "drvXy566.h"

static short ai_num_cards[NUM_TYPES] = {0, 0, 0};
static short ai_num_channels[NUM_TYPES] = {32,16,16};                 /* channels per card */
static unsigned short ai_addrs[NUM_TYPES] = {0x6000,0x7000,0xe000};
static long ai_memaddrs[NUM_TYPES] = {0x000000,0x040000,0x0c0000};

static int AI566_VNUM = 0xf8;  /* Xycom 566 Differential Latched */

static short wf_num_cards[] = {0};
static short wf_num_channels[] = {1};
static unsigned short wf_addrs[] = {0x9000};
static unsigned short wf_armaddrs[] = {0x5400};
static long wf_memaddrs[] = {0x080000};

/* Number of columns used in io_report. */
#define IOR_MAX_COLS 4

#if 0
static struct {char ** ppc; char*pc;} RCSID = {&(RCSID.pc),
    "drvXy566.c,v 1.8 2004/01/21 20:41:23 mrk Exp"};
#endif

/*
 * Code Portions
 *
 * xy566DoneTask   Task to handle data take completion for the 566 waveform
 * wf_xy566_init      Initializes the 566 waveform cards
 * senb/senw      Writes to the 566 where the call provides a req'd delay
 */


/* forward references */
static long xy566_report(int level);
static long xy566_init(void);
static long ai_xy566_init(void);
static long wf_xy566_init(void);
static long ai_xy566_io_report(int level);
static long wf_xy566_io_report(int level);
static void xy566_reset(void *); 
static void rval_convert(unsigned short *rval);
static void xy566_rval_report(short card, short type);



struct {
   long   number;
   DRVSUPFUN   report;
   DRVSUPFUN   init;
} drvXy566={
   2,
   xy566_report,
   xy566_init};
epicsExportAddress(drvet,drvXy566);

static long xy566_init(void)
{
    ai_xy566_init();
    if(wf_num_cards[XY566WF] > 0) wf_xy566_init();
    return(0);
}

static long xy566_report(int level)
{
    ai_xy566_io_report(level);
    wf_xy566_io_report(level);
    return(0);
}

#define MAX_SE_CARDS   (ai_num_cards[XY566SE])
#define MAX_DI_CARDS   (ai_num_cards[XY566DI])
#define MAX_DIL_CARDS  (ai_num_cards[XY566DIL])

#define XY566_MEM_INCR  0x10000 /* size of data memory area */


void xy566_set_gain(int, int);

/* gain for all channels of each card = 1 by default */
/* up to 8 cards supported */
/* this is a hack that assumes only one type of card installed */
/* so if you want different types of cards installed and you want */
/* different gain settings for the different types, more hacking is necessary */
int gain_select[8] = {0,0,0,0,0,0,0,0}; 

/* memory structure of the 566 interface */
typedef volatile struct  {              /* struct XVME 566 */
        char            dum[0x80];      /* odd bytes 1 - 3f contain
                                           module identification */
        unsigned short  a566_csr;       /* control status register */
        unsigned char   soft_start;     /* software start register */
        unsigned char   int_vect;       /* interrupt vector register */
        unsigned short  dram_ptr;       /* pointer to data ram */
        char            dum1;           /* unused */
        unsigned char   sram_ptr;       /* sequence ram pointer */
        char            dum2[0xc0 - 0x88];
        unsigned short  stc_data;       /* timing chip data port */
        unsigned short  stc_control;    /* timing chip control port */
        char            dum3[0x101 - 0xc4];
        unsigned char   gram_base;      /* base of gain ram 101,103 to 13f */
        char            dum5[0x201 - 0x102];
        unsigned char   sram_base;      /* base of sequence ram 210,203 to 3ff */
        char            dum6[0x3ff -0x202];
        unsigned char   card_number;    /* logical card number */
} ai566Regs_t;

/* memory structure of the 566 interface */
typedef volatile struct  {              /* struct XVME 566 */
        char            dum[0x80];      /* odd bytes 1 - 3f contain
                                           module identification */
        unsigned short  csr;            /* control status register */
        unsigned char   soft_start;     /* software start register */
        unsigned char   int_vect;       /* interrupt vector register */
        unsigned short  dram_ptr;       /* pointer to data ram */
        char            dum1;           /* unused */
        unsigned char   sram_ptr;       /* sequence ram pointer */
        char            dum2[0xc0 - 0x88];
        unsigned short  stc_data;       /* timing chip data port */
        unsigned short  stc_control;    /* timing chip control port */
        char            dum3[0x101 - 0xc4];
        unsigned char   gram_base;      /* base of gain ram 101,103,..
                                           ... to 13f */
        char            dum5[0x201 - 0x102];
        unsigned char   sram_base;      /* base of sequence ram 210,203,
                                           ... to 3ff */
        char            dum6[0x400 -0x202];
} wf085Regs_t;

/* arrays which keep track of which cards are found at initialization */
ai566Regs_t   **pai_xy566se;
ai566Regs_t   **pai_xy566di;
ai566Regs_t   **pai_xy566dil; 
unsigned short   **pai_xy566se_mem;
unsigned short   **pai_xy566di_mem;
unsigned short   **pai_xy566dil_mem;
static IOSCANPVT *paioscanpvt;


/* reset the counter interrupt                              0x8000 */
/* reset the sequence interrupt                             0x2000 */
/* enable the sequence interrupt                            0x1000 */
/* reset the trigger clock interrupt                        0x0800 */
/* enable the sequence controller             0x0100 */
/* read values into first 32 words on each read             0x0040 */
/* read in sequential mode  (bit 0x0020 == 0)               0x0000 */
/* interrupt enable                                         0x0008 */
/* leds green-on red-off                                    0x0003 */

#define   XY566L_CSR   0xb94b
#define XY566_INT_LEVEL 6


/* max card and channel definitions move to module types.h */
#define MAX_WF_XY_CARDS        (wf_num_cards[XY566WF]) 

/* card interface */
#define WF_XY566_BASE           (wf_addrs[XY566WF]) /* XYCOM 566 waveform */
#define WF_XY085_BASE           (wf_armaddrs[XY566WF]) /* XYCOM 085 arm */

/* Data RAM area into which the 566 stores the latched data */
/* This needs to be different for each type of IO card */
#define WF_XY566_MEMBASE        ((unsigned short *)0x1080000)

/* figure out what these are */
#define WF566_MEM_INCR  0x10000         /* 65535 bytes per waveform */
#define WF566_VNUM      0xf1            /* this is currently incorrect */


#define WF_READ         0x00            /* read a waveform */
#define WF_ARM          0x01            /* arm a waveform */
#define WF_LATCH        0x02            /* latch a waveform */
#define WF_SETUP        0x03            /* set up a waveform */

/* xycom 085 encoder pulse mechanism */
#define XY_ARM     0x10    /* arm the encoder pulse circuitry */
#define XY_BUSY    0x20    /* indicates the waveform is still being taken */
#define XY_LED          0x3     /* set the Xycom status LEDs to OK */
#define XY_SOFTRESET    0x10    /* reset the IO card */


/* arrays which keep track of which cards are found at initialization */
ai566Regs_t   **pwf_xy566;
wf085Regs_t   **pwf_xy085;
char          **pwf_mem;
 

/* the routine to call when the data is read and the argument to send */
void          (**proutine)(void *);
unsigned int   **pargument;

/* VME memory Short Address Space is set up in gta_init */

epicsThreadId      wfDoneId;   /* waveform done task ID */


/*      The following two subroutines introduce a delay between
 *      successive writes to the 566. This is necessary for some
 *      parts of the card (i.e. the AM9513). It also insures
 *      that a value is actually written, instead of compiler
 *      generated bset or bclr instructions.
 */
static void senw (unsigned short volatile *addr, unsigned short val)
{
        *addr = val;
}
 
static void senb (unsigned char volatile *addr, unsigned char val)
{
        *addr = val;
}

static void ai566_intr(void *p)
{
   ai566Regs_t  *ap;
   int cardnum = *(int *)p; 

   ap = pai_xy566dil[cardnum];

   scanIoRequest(paioscanpvt[cardnum]);

   /* reset the CSR - needed to allow next interrupt */
   senw(&ap->a566_csr,XY566L_CSR);
        return;
}

/*
 * ai_XY566_INIT
 *
 * intialize the xycom 566 analog input cards
 */
static long ai_xy566_init_cards(
    ai566Regs_t   ***pppcards_present, 
    unsigned int     base_addr,
    short            num_channels,
    short            num_cards,
    unsigned int     paimem,
    short         ***pppmem_present
) {
   short         shval;
   short         card, chan;
   ai566Regs_t  *pai566;   /* memory location of cards */
   char         *pai566io;   /* mem loc of I/O buffer */
   short         status;
   ai566Regs_t **ppcards_present;
   short       **ppmem_present;

if(num_cards > 0) {  /* Don't do any of this if we don't have any cards! */

      *pppcards_present = (ai566Regs_t **)callocMustSucceed(num_cards, sizeof(**pppcards_present), "XY566: Can't allocate memory");

      *pppmem_present = (short **)callocMustSucceed(num_cards, sizeof(**pppmem_present), "XY566: Can't allocate memory");

      ppcards_present = *pppcards_present;
      ppmem_present = *pppmem_present;

      /* map the io cards into contiguous short address space */
      status = devRegisterAddress("drvXy566regs", atVMEA16, base_addr, num_cards * sizeof(ai566Regs_t), (volatile void **)&pai566);
      if(status != OK){
         errlogPrintf("%s: failed to map XY566 A16 base addr A16=%x\n", __FILE__, base_addr);
         return ERROR;
      }

      /* map the io cards into contiguous standard address space */
      /*status = sysBusToLocalAdrs(VME_AM_STD_SUP_DATA,(char *)paimem, (char **)&pai566io);*/
      status = devRegisterAddress("drvXy566mem", atVMEA24, paimem, num_cards * XY566_MEM_INCR,  (volatile void **)&pai566io);
      if(status != OK){
         errlogPrintf( "%s: failed to map XY566 A24 base addr A24=%x\n",  __FILE__, paimem);
         return ERROR;
      }


      /* mark each card present into the card present array */
      for (card=0; card<num_cards; ++card, ++pai566, ++ppcards_present, pai566io+=XY566_MEM_INCR, ++ppmem_present) {
         if(devReadProbe(sizeof(short), pai566, &shval) != OK) {
            *ppcards_present = 0;
            continue;
         }
         if(devReadProbe(sizeof(short), pai566io, &shval) != OK) {
            *ppcards_present = 0;
            continue;
         }

         *ppcards_present = pai566;
         *ppmem_present = (short *)pai566io;

         /* reset the Xycom 566 board */
         senw(&pai566->a566_csr,0x00);           /* off seq control */
         senw(&pai566->a566_csr,XY_SOFTRESET);   /* reset */
         senw(&pai566->a566_csr,XY_LED);         /* reset off,red off,green on */

         /* Am9513 commands */
         /* initialize the Am9513 chip on the XY566 */
         senw(&pai566->stc_control,0xffff);   /* master reset */
         senw(&pai566->stc_control,0xff5f);   /* disarm all counters */
         senw(&pai566->stc_control,0xffef);   /* 16 bit mode */

         /* master mode register */
         senw(&pai566->stc_control,0xff17);   /* select master mode reg */
         senw(&pai566->stc_data,0x2200);      /* 16 bit, divide by 4 */

         /* counter two is used to set the time between sequences */
         senw(&pai566->stc_control,0xff02);   /* sel counter 2 */
         senw(&pai566->stc_data,0x0b02);      /* TC toggle mode */
         senw(&pai566->stc_control,0xffea);   /* TC output high */

         /* counter four is used as time between sequence elements */
         senw(&pai566->stc_control,0xff04);   /* sel counter 4 */
         senw(&pai566->stc_data,0x0b02);      /* TC toggle mode */
         senw(&pai566->stc_control,0xffec);   /* TC output high */

         /* counter five is used as an event counter */
         senw(&pai566->stc_control,0xff05);   /* sel counter 5 */
         senw(&pai566->stc_data,0x0b02);      /* TC toggle mode */
         senw(&pai566->stc_control,0xffed);   /* TC output high */

         /* set time between sequences */
         senw(&pai566->stc_control,0xff04);   /* sel counter 4 */
         senw(&pai566->stc_data,0x9525);      /* see Am9513A manual */
         senw(&pai566->stc_data,0x0014);      /* downcount value */
         senw(&pai566->stc_control,0xff68);   /* load & arm cntr 4 */

         senw(&pai566->stc_control,0xff05);   /* sel counter 4 */
         senw(&pai566->stc_data,0x97ad);      /* see Am9513A manual */
         senw(&pai566->stc_data,0x0100);      /* downcount value */
         senw(&pai566->stc_control,0xff70);   /* load & arm cntr 4 */
         /* end of the Am9513 commands */

         /* for each channel set gain and place into the scan list */
         for (chan=0; chan<num_channels; ++chan) {
            /* all channels on same card get same gain setting */
            senb((&pai566->gram_base + chan*2), gain_select[card]); 
            /* end of sequnce = 0x80 | channel */
            /* stop           = 0x40 | channel */
            senb((&pai566->sram_base+chan*2),(chan==num_channels-1)? chan|0x80:chan);
         }
         senw(&pai566->dram_ptr, 0);      /* data ram at 0 */
         senb(&pai566->sram_ptr, 0);      /* seq ram also at 0 */

           /* set the Xycom 566 board */
           /* reset the counter interrupt                              0x8000 */
           /* reset the sequence interrupt                             0x2000 */
           /* reset the trigger clock interrupt                        0x0800 */
           /* enable the sequence controller                           0x0100 */
           /* read values into first 32 words on each read             0x0040 */
           /* read in sequential mode  (bit 0x0020 == 0)               0x0000 */
           /* leds green-on red-off                                    0x0003 */
         senw(&pai566->a566_csr,0xa943 );   /* init csr */

         /* latch in the first bunch of data and start continuous scan */
         senb(&pai566->soft_start,0);
      } 
   } /* if(num_cards > 0) */
   return OK;
} 

/*
 * AI_XY566L_INIT
 *
 * intialize the xycom 566 latched analog input card
 */
static long ai_xy566l_init_cards(
   ai566Regs_t   ***pppcards_present,
   unsigned int     base_addr,
   short            num_channels,
   short            num_cards,
   unsigned int     paimem,
   unsigned short ***pppmem_present
) {
   short            shval;
   short            card, chan;
   ai566Regs_t     *pai566;     /* memory location of cards */
   char            *pai566io;   /* mem loc of I/O buffer */
   int              status;
   ai566Regs_t    **ppcards_present;
   unsigned short **ppmem_present;



   if(num_cards > 0) {    /* Don't do any of this if no cards! */
      *pppcards_present = (ai566Regs_t **) callocMustSucceed(num_cards, sizeof(**pppcards_present), "XY566: Can't allocate memory");

      paioscanpvt = (IOSCANPVT *)callocMustSucceed(num_cards, sizeof(*paioscanpvt), "XY566: Can't allocate memory");
   
      for(card=0; card<num_cards; ++card) {
         paioscanpvt[card] = NULL;
         scanIoInit(&paioscanpvt[card]);
      }
   

      *pppmem_present = (unsigned short **)callocMustSucceed(num_cards, sizeof(**pppmem_present), "XY566: Can't allocate memory");

      ppcards_present = *pppcards_present;
      ppmem_present = *pppmem_present;

      /* map the io cards into contiguous  short address space */
      /* if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,(char *)base_addr, (char **)&pai566)) != OK){ */
      if((status = devRegisterAddress( "drvXy566lregs", 
                                        atVMEA16, 
                                        base_addr, 
                                        num_cards * sizeof(ai566Regs_t), 
                                        (volatile void **)&pai566)) != OK )  {
          errlogPrintf(   "%s: failed to map XY566 (latched) A16 base addr A16=%x\n", __FILE__, base_addr);
          return ERROR;
      }

      /* map the io cards into contiguous standard address space */
      /* if((status = sysBusToLocalAdrs(VME_AM_STD_SUP_DATA,(char *)paimem, (char **)&pai566io)) != OK){ */
      if((status = devRegisterAddress( "drvXy566lmem", 
                                        atVMEA24, 
                                        paimem, 
                                        num_cards * XY566_MEM_INCR, 
                                        (volatile void **)&pai566io)) != OK )  {
         errlogPrintf(   "%s: failed to map XY566 (latched) A24 base addr A24=%x\n",  __FILE__, paimem);
         return ERROR;
      }

      epicsAtExit(xy566_reset, (void *)NULL);
      /* rebootHookAdd(xy566_reset); */

      /* mark each card present into the card present array */
      for (card=0; card<num_cards; ++card, ++pai566, ++ppcards_present, pai566io+=XY566_MEM_INCR, ++ppmem_present){
         int *parg;  /* card number argument to the interrupt service routine */

         if (devReadProbe(sizeof(short), pai566, &shval)) {
            *ppcards_present = 0;
            continue;
         }

         if (devReadProbe(sizeof(short), pai566io,&shval) != OK){
            *ppcards_present = 0;
            continue;
         }
         
         *ppcards_present = pai566;
         *ppmem_present = (unsigned short *)pai566io;

         /* put the card number in the dual ported memory */
         senb(&pai566->card_number, card);

         /* set up the interrupt vector */
         /* taken from the XYCOM-566 Manual. Figure 4-6  Page 4-19 */
         pai566->int_vect = AI566_VNUM + card;
  
         parg = callocMustSucceed(1, sizeof(*parg), "XY566: Can't allocate memory"); 
         *parg = card;
         devConnectInterruptVME(AI566_VNUM + card, ai566_intr, &parg);
         devEnableInterruptLevelVME(XY566_INT_LEVEL); 

         /* reset the Xycom 566 board */
         senw(&pai566->a566_csr,0x00);      /* off seq control */
         senw(&pai566->a566_csr,XY_SOFTRESET);   /* reset */
         senw(&pai566->a566_csr,XY_LED);      /* reset off,red off,green on */

         /* Am9513 commands */
         /* initialize the Am9513 chip on the XY566 */
         senw(&pai566->stc_control,0xffff);   /* master reset */
         senw(&pai566->stc_control,0xff5f);   /* disarm all counters */
         senw(&pai566->stc_control,0xffef);   /* 16 bit mode */

         /* master mode register */
         senw(&pai566->stc_control,0xff17);   /* select master mode reg */
         senw(&pai566->stc_data,0x2200);      /* 16 bit, divide by 4 */

         /* counter two is used to set the time between sequences */
         senw(&pai566->stc_control,0xff02);   /*sel counter 2 */
         senw(&pai566->stc_data,0x0b02);      /* TC toggle mode */
         senw(&pai566->stc_control,0xffea);   /* TC output high */

         /* counter four is used as time between sequence elements */
         senw(&pai566->stc_control,0xff04);   /* sel counter 4 */
         senw(&pai566->stc_data,0x0b02);      /* TC toggle mode */
         senw(&pai566->stc_control,0xffec);   /* TC output high */

         /* counter five is used as an event counter */
         senw(&pai566->stc_control,0xff05);   /* sel counter 5 */
         senw(&pai566->stc_data,0x0b02);      /* TC toggle mode */
         senw(&pai566->stc_control,0xffed);   /* TC output high */

         /* set time between sequences */
         senw(&pai566->stc_control,0xff04);   /* sel counter 4 */
         senw(&pai566->stc_data,0x9525);      /* see Am9513A manual */
         senw(&pai566->stc_data,0x0014);      /* downcount value */
         senw(&pai566->stc_control,0xff68);   /* load & arm cntr 4 */

         senw(&pai566->stc_control,0xff05);   /* sel counter 4 */
         senw(&pai566->stc_data,0x97ad);      /* see Am9513A manual */
         senw(&pai566->stc_data,0x0100);      /* downcount value */
         senw(&pai566->stc_control,0xff70);   /* load & arm cntr 4 */
         /* end of the Am9513 commands */

         /* for each channel set gain and place into the scan list */
         for (chan=0; chan<num_channels; ++chan) {
            /* all channels on same card get same gain setting */
            senb((&pai566->gram_base + chan*2), gain_select[chan]); 

            /* end of sequnce = 0x80 | channel */
            /* stop           = 0x40 | channel */
            /* interrupt      = 0x20 | channel */
            senb((&pai566->sram_base+chan*2),(chan==num_channels-1)? chan|0xe0:chan);
         }
         senw(&pai566->dram_ptr,0);      /* data ram at 0 */
         senb(&pai566->sram_ptr,0);      /* seq ram also at 0 */

               /* initialize the control status register */
         /* reset the sequence interrupt                             0x2000 */
         /* enable the sequence interrupt                            0x1000 */
         /* reset the trigger clock interrupt                        0x0800 */
         /* enable the sequence controller                           0x0100 */
         /* read values into first 32 words on each read             0x0040 */
         /* read in sequential mode  (bit 0x0020 == 0)               0x0000 */
         /* interrupt enable                                         0x0008 */
         /* leds green-on red-off                                    0x0003 */
         senw(&pai566->a566_csr,XY566L_CSR);
      }

   } /* if (num_cards > 0) */
   return OK;
}


/*
 *   ai_xy566_init ()
 *
 *   Initialize all VME analog input cards
 */

static long ai_xy566_init(void)
{
   /* intialize the Xycom 566 Unipolar Single Ended Analog Inputs */
   ai_xy566_init_cards(&pai_xy566se,ai_addrs[XY566SE],ai_num_channels[XY566SE],
           ai_num_cards[XY566SE],ai_memaddrs[XY566SE],(short ***)&pai_xy566se_mem); 

   /* intialize the Xycom 566 Unipolar Differential Analog Inputs */
   ai_xy566_init_cards(&pai_xy566di,ai_addrs[XY566DI],ai_num_channels[XY566DI],
      ai_num_cards[XY566DI],ai_memaddrs[XY566DI],(short ***)&pai_xy566di_mem); 


   /* intialize the Xycom 566 Unipolar Differential Analog Inputs Latched */
   ai_xy566l_init_cards(&pai_xy566dil,ai_addrs[XY566DIL],ai_num_channels[XY566DIL],
      ai_num_cards[XY566DIL],ai_memaddrs[XY566DIL],(unsigned short ***)&pai_xy566dil_mem); 

   return (0);
}


int ai_xy566_getioscanpvt(unsigned short card, IOSCANPVT *scanpvt)
{
   if((card<=(unsigned short)MAX_DIL_CARDS) && paioscanpvt[card]) 
      *scanpvt = paioscanpvt[card];
   return(0);
}

int ai_xy566_driver(
    short           card,
    short           chan,
    unsigned int    type,
    unsigned short *prval
) {
   /* check on the card and channel number as kept in module_types.h */
   if (card >= ai_num_cards[type]) return(-1);
   if (chan >= ai_num_channels[type]) return(-2);

   switch (type){

      case (XY566SE):
      {

         /* check card specified exists */
         if (pai_xy566se[card] == 0) return(-1);

         /* read the value from the Xycom data RAM area */
         *prval = pai_xy566se_mem[card][chan];

         return (0);
      }

      case (XY566DI):
      {

         /* check card specified exists */
         if (pai_xy566di[card] == 0) return(-1);

         /* read the value form the Xycom data RAM area */

         *prval = pai_xy566di_mem[card][chan];

         rval_convert(prval);
         return (0);
      }

      case (XY566DIL):
      {

         /* check card specified exists */
         if (pai_xy566dil[card] == 0) return(-1);

         /* read the value form the Xycom data RAM area */
         *prval = pai_xy566dil_mem[card][chan];

         rval_convert(prval);
         return (0);
      }


  }

  return (-3);

}

/*
 * rval_convert 
 *          For 566_card -  values for XY566DI and XY566DIL
 *     come from the card as signed hex numbers( -0x7ff to +0x7ff).
 *     This subrountine converts them to unsigned hex numbers (0x0 -
 *     0xfff.  Then it strips off the sign bit.
 *
*/
static void rval_convert(unsigned short *rval)
{
   
   *rval = *rval + 0x800; 

    /* remove the sign bits. */

   *rval &= 0xfff;
}

/*
 *
 *     xy566_reset- Called for ctl X reboot.  The idea is to disable
 *     bits 3 and 12 of the CSR.
 *
*/

static void xy566_reset(void *pvt){
   unsigned short csr_val,shval;
   int i; 
   ai566Regs_t   *pai566;   /* memory location of cards */
   int status;

   status = devRegisterAddress("xy566_reset", atVMEA16, ai_addrs[XY566DIL], ai_num_cards[XY566DIL] * sizeof(ai566Regs_t), (void *)&pai566);
   if (status != OK){
      errlogPrintf("%s: unable to map A16 XY566 base\n", __FILE__);
   }

   for (i=0;i<ai_num_cards[XY566DIL]; i++, pai566++ ){
      if (devReadProbe(sizeof(short), pai566, &shval) != OK){
         continue;
      }
      csr_val =(unsigned short )XY566L_CSR;
      csr_val &= 0xeff7;
      pai566->a566_csr = csr_val;
   }
}




/*
 *  io_report (or dbior) subroutine for 566 card.
 *
 * 
 */
static long ai_xy566_io_report(int level)
{
   int i;
    
   for (i = 0; i < MAX_SE_CARDS; i++){
      if (pai_xy566se[i]){   
         epicsPrintf("AI: XY566-SE:\tcard %d\n",i);
         if (level == 1){
            xy566_rval_report(i,XY566SE); 
         }    
      }
    }

   for (i = 0; i < MAX_DI_CARDS; i++){
      if (pai_xy566di[i]){   
         epicsPrintf("AI: XY566-DI:\tcard %d\n",i);
         if (level == 1){
            xy566_rval_report(i,XY566DI); 
         } 
      }
   }
       
   for (i = 0; i < MAX_DIL_CARDS; i++){
      if (pai_xy566dil[i]){
         epicsPrintf("AI: XY566-DIL:\tcard %d\n",i); 
         if (level == 1){
            xy566_rval_report(i,XY566DIL); 
         } 
      }
   }
   return OK;
}

/*
 *  xy566_rval_report -reports the raw value for every channel on the card  
 * 
 *  called by ai_xy566_io_report if level is 1 
 *
 */
static void xy566_rval_report(short card, short type)
{ 
   unsigned short   chan, num_chans, val;

   epicsPrintf("\n");

   epicsPrintf("Raw values:\n");
   num_chans = ai_num_channels[type];
   for(chan=0; chan<num_chans; ++chan) {
      if(ai_xy566_driver(card,chan,type,&val) == 0){       
         epicsPrintf("Chan %02d = 0x%04x\t",chan,val);
         if( (chan%IOR_MAX_COLS) == (IOR_MAX_COLS-1)) epicsPrintf("\n");
      } else epicsPrintf("READ_ALARM\n");  
   }
   epicsPrintf("\n");
}



/*
 * xy566_driver
 *
 */
int xy566_driver(
    unsigned short slot,
    void   (*pcbroutine)(void*),
    void   *parg  /* number of values read */
) {
   register ai566Regs_t *pwf566;
   register wf085Regs_t *pwf085;

   /* slot range checking occurs in wf_driver.c */

   /* is the Xycom 566 card present */
   if ((pwf566 = pwf_xy566[slot]) == 0)
         return(-1);
   if ((pwf085 = pwf_xy085[slot]) == 0)
         return(-1);

   /* armed already by someone else */
   if (proutine[slot] != NULL)
      return(-2);   /* by someone else */

   /* set the Xycom 566 board */
   senw(&pwf566->dram_ptr,0); /* RAM pointer to 0 */
    senw((unsigned short *)&pwf566->sram_ptr,0); /* sequence pointer to 0 */

   /* keep the pointer to the value field */
   proutine[slot] = pcbroutine;
   pargument[slot] = parg;

   /* arm the encoder pulse mechanism */
   senw(&pwf085->csr,XY_ARM | XY_LED | 0x20);   /* set high */
   senw(&pwf085->csr,XY_LED | 0x20);      /* set low */

   return(0);
}

/*
 * xy566DoneTask
 *
 * polls the busy bit on the Xycom 566 latched waveform records
 * The busy bit is set externally when data collection is completed
 */
static void xy566DoneTask(void *parm)
{
   void               (**pproutines)(void *);
   unsigned int       (*pcbroutine)();
   short              i;

   while(TRUE){
      pproutines = proutine;
      for (i=0;  i<MAX_WF_XY_CARDS; i++,pproutines++){

         /* check if the waveform is armed */
         if (*pproutines == 0) continue;

         /* check if the data is taken */
         if ((pwf_xy085[i]->csr & XY_BUSY) == 0){
            /* callback routine when data take completed */
            /* (unsigned int *)pcbroutine = *pproutines; */ /* WHAT?? */
            pcbroutine = (void *)*pproutines;
            (*pcbroutine) (pargument[i],pwf_xy566[i]->dram_ptr,pwf_mem[i]);

            /* reset for someone else to claim */
            *pproutines = 0;
            pargument[i] = 0;
         }
      }

      /* sleep for .1 second - system degrade will slow this task */
      /* that's OK                      */
      epicsThreadSleep(0.05);
      /* taskDelay(2); */  /* ges: changed from 6 ticks to 2 ticks 2/4/91 */
      /* mdw: changed taskDelay() to epicsThreadSleep() 2016-03-15 */
   }
}


/*
 * XY566_INIT
 *
 * intialize the xycom 566 waveform input card
 */
static long wf_xy566_init(void)
{
   ai566Regs_t   **pcards_present = pwf_xy566;
   wf085Regs_t   **parms_present = pwf_xy085;
   char          **pmem_present = pwf_mem;
   short           shval,status;
   short           i,got_one;
   ai566Regs_t    *pwf566;   /* VME location of cards */
   wf085Regs_t    *pwf085;   /* VME location of arm */
   char           *pwfMem;   /* VME 566 memory buffer */

   pwf_xy566 = (ai566Regs_t **)callocMustSucceed(wf_num_cards[XY566WF], sizeof(*pwf_xy566), "XY566: Can't allocate memory");
    
   pwf_xy085 = (wf085Regs_t **)callocMustSucceed(wf_num_cards[XY566WF], sizeof(*pwf_xy085), "XY566: Can't allocate memory");

   pwf_mem = (char **)callocMustSucceed(wf_num_cards[XY566WF], sizeof(*pwf_mem), "XY566: Can't allocate memory");

   pargument = (unsigned int **)callocMustSucceed(wf_num_cards[XY566WF], sizeof(*pargument), "XY566: Can't allocate memory");

   proutine = (void *)callocMustSucceed(wf_num_cards[XY566WF], sizeof(*proutine), "XY566: Can't allocate memory");

   pcards_present = pwf_xy566;
   parms_present = pwf_xy085;
   pmem_present = pwf_mem;

   /* map the io card into the VME short address space */
   status = devRegisterAddress("wf_xy566", atVMEA16, WF_XY566_BASE, wf_num_cards[XY566WF]*sizeof(ai566Regs_t), (void *)&pwf566);
   if(status < 0){
      errlogPrintf("%s: unable to map A16 XY566 base\n", __FILE__);
      return ERROR;
   }

  devRegisterAddress("wf_xy085", atVMEA16, WF_XY085_BASE, wf_num_cards[XY566WF]*sizeof(wf085Regs_t), (void *)&pwf085); 
       if(status < 0){
          errlogPrintf("%s: unable to map A16 XY085 base\n", __FILE__);
          return ERROR;
       }

      devRegisterAddress("wf_mem", atVMEA24, wf_memaddrs[XY566WF], wf_num_cards[XY566WF]*WF566_MEM_INCR, (void *)&pwfMem);
      if (status != OK){
         errlogPrintf("%s: unable to map A24 XY566 base\n", __FILE__);
         return ERROR;
       }

    /* mark each card present into the card present array */
    got_one = 0;
    for (i = 0;
      i < wf_num_cards[XY566WF];
      i++, pwf566++,pwf085++,pwfMem += XY566_MEM_INCR, pcards_present += 1) {

      /* is the Xycom 566 here */
      if (devReadProbe(sizeof(short), pwf566, &shval) != OK){
         *pcards_present = 0;
         continue;
      }
      /* is the Xycom 566 memory here */
      if (devReadProbe(sizeof(short), pwfMem, &shval) != OK){
         *pcards_present = 0;
         continue;
      }
      /* is the Xycom 085 used as the arming mechanism present */
      if (devReadProbe(sizeof(short), pwf085, &shval) != OK){
         *pcards_present = 0;
         continue;
      }

      got_one = 1;
      *pcards_present = pwf566;
      *parms_present = pwf085;
      *pmem_present = pwfMem;

      /* taken from the XYCOM-566 Manual. Figure 4-6  Page 4-19 */
      /* reset the Xycom 566 board */
      senw (&pwf566->a566_csr,0x00);      /* off seq control */
      senw (&pwf566->a566_csr,XY_SOFTRESET);   /* reset */
      senw (&pwf566->a566_csr,XY_LED);   /* reset off,red off,green on */

      /* Am9513 commands */
      /* initialize the Am9513 chip on the XY566 */
      senw (&pwf566->stc_control, 0xffff);   /* master reset */
      senw(&pwf566->stc_control, 0xff5f);   /* disarm all counters */
      senw(&pwf566->stc_control, 0xffef);   /* 16 bit mode */

      /* master mode register */
      senw(&pwf566->stc_control, 0xff17);   /* select master mode reg */
      senw(&pwf566->stc_data, 0x2200);   /* 16 bit, divide by 4 */

      /* counter two is used to set the time between sequences */
      senw(&pwf566->stc_control, 0xff02);   /*sel counter 2 */
      senw(&pwf566->stc_data, 0x0b02);   /* TC toggle mode */
      senw(&pwf566->stc_control, 0xffea);   /* TC output high */

      /* counter four is used as time between sequence elements */
      senw(&pwf566->stc_control, 0xff04);   /* sel counter 4 */
      senw(&pwf566->stc_data, 0x0b02);   /* TC toggle mode */
      senw(&pwf566->stc_control, 0xffec);   /* TC output high */

      /* counter five is used as an event counter */
      senw(&pwf566->stc_control, 0xff05);   /* sel counter 5 */
      senw(&pwf566->stc_data, 0x0b02);   /* TC toggle mode */
      senw(&pwf566->stc_control, 0xffed);   /* TC output high */

      /* set counter 4 */
      /* active high level gate N         0x8000      */
      /* count on the falling edge         0x1000      */
      /* SRC 5               0x0500      */
      /* disable special gate            0x0080 = 0   */
      /* reload from load            0x0040 = 0   */
      /* count repetitively            0x0020 = 1   */
      /* binary count               0x0010 = 0   */
      /* count down               0x0008 = 0   */
      /* active low terminal count pulse      0x0007 = 5   */
      senw(&pwf566->stc_control,0xff04);   /* sel counter 4 */
      senw(&pwf566->stc_data,0x9525);      /* see comment above*/
      senw(&pwf566->stc_data,0x0014);      /* downcount value */
      senw(&pwf566->stc_control,0xff68);   /* load & arm cntr 4 */

      /* set counter 5 */
      /* active high level gate N         0x8000      */
      /* count on the falling edge         0x1000      */
      /* GATE 2               0x0700      */
      /* enable special gate            0x0080 = 1   */
      /* reload from load            0x0040 = 0   */
      /* count repetitively            0x0020 = 1   */
      /* binary count               0x0010 = 0   */
      /* count up               0x0008 = 1   */
      /* active low terminal count pulse      0x0007 = 5   */
      senw(&pwf566->stc_control,0xff05);   /* sel counter 5 */
      senw(&pwf566->stc_data,0x97ad);   /* see comment above */
      senw(&pwf566->stc_data,0x0100);   /* count value */
      senw(&pwf566->stc_control,0xff70);   /* load & arm cntr 5*/
      /* end of the Am9513 commands */

      /* Xycom gain RAM */
      senb(&pwf566->gram_base,0);      /* set gain to 1 ch0*/

      /* Xycom sequence RAM */
      senb(&pwf566->sram_base,0xc0);   /* read only the 0th channel */

      /* Xycom data RAM index */
      senw(&pwf566->dram_ptr,0);      /* data ram at 0 */

      /* Xycom sequential RAM index */
      senb(&pwf566->sram_ptr,0);      /* seq ram also at 0 */

      /* set the Xycom 566 board */
      /* reset the counter interrupt                              0x8000 */
      /* reset the sequence interrupt                             0x2000 */
      /* reset the trigger clock interrupt                        0x0800 */
      /* enable the sequence controller                           0x0100 */
      /* read values into data RAM contiguously (bit 0x0040 == 0) 0x0000 */
      /* read in sequential mode  (bit 0x0020 == 0)               0x0000 */
      /* leds green-on red-off                                    0x0003 */
      senw(&pwf566->a566_csr,0xa903);   /* init csr */

      /* initialize the xycom 085 used as the arming mechanism */
      /* leds green-on red-off          0x0003 */
      senw(&pwf085->csr,XY_LED | 0x20);   /* init csr */
   }

   /* start the 566 waveform readback task */
   if (got_one)
      wfDoneId = epicsThreadCreate("xy566_wf_rdbk_task",
                                 epicsThreadPriorityMedium, 
                                 epicsThreadGetStackSize(epicsThreadStackMedium), 
                                 xy566DoneTask,
                                 NULL);
   taskwdInsert(wfDoneId,NULL,NULL);

   return 0;
}


/*      
 * XY566_IO_REPORT
 *
 */
static long wf_xy566_io_report(int level)
{
   int i;

   /* report all of the xy566 waveform inputs present */
   for (i = 0; i < wf_num_cards[XY566WF]; i++)
      if (pwf_xy566[i]){
                 epicsPrintf("WF: XY566:      card %d\n",i);
                }
                           
        return(0);
}

/* this has to be called BEFORE the Configure routine fpr each card */
void xy566_set_gain(int card, int val)
{
   gain_select[card] == val;
}


int xy566SEConfig(unsigned int ncards, unsigned int nchannels,
    unsigned int base, unsigned int memory)
{
    ai_num_cards[XY566SE] = ncards;
    ai_num_channels[XY566SE] =  nchannels;
    ai_addrs[XY566SE] = base;
    ai_memaddrs[XY566SE] = memory;
    return 0;
}

int xy566DIConfig(unsigned int ncards, unsigned int nchannels,
    unsigned int base, unsigned int memory)
{
    ai_num_cards[XY566DI] = ncards;
    ai_num_channels[XY566DI] =  nchannels;
    ai_addrs[XY566DI] = base;
    ai_memaddrs[XY566DI] = memory;
    return 0;
}

int xy566DILConfig(unsigned int ncards, unsigned int nchannels,
    unsigned int base, unsigned int memory)
{
    ai_num_cards[XY566DIL] = ncards;
    ai_num_channels[XY566DIL] =  nchannels;
    ai_addrs[XY566DIL] = base;
    ai_memaddrs[XY566DIL] = memory;
    return 0;
}

int xy566WFConfig(unsigned int ncards, unsigned int nchannels,
    unsigned int base, unsigned int memory, unsigned int armaddr,
    unsigned int vector)
{
    wf_num_cards[XY566WF] = ncards;
    wf_num_channels[XY566WF] = nchannels;
    wf_addrs[XY566WF] = base;
    wf_memaddrs[XY566WF] = memory;
    wf_armaddrs[XY566WF] = armaddr;
    AI566_VNUM = vector;
    return 0;
}


/* make xy566SEConfig() available to the ioc shell */
static const iocshArg xy566SEConfigArg0 = {"number of cards",                iocshArgInt};
static const iocshArg xy566SEConfigArg1 = {"number of channels",             iocshArgInt};
static const iocshArg xy566SEConfigArg2 = {"VME A16 registers base address", iocshArgInt};
static const iocshArg xy566SEConfigArg3 = {"VME A24 memory base address",    iocshArgInt};
static const iocshArg *xy566SEConfigArgs[] = {
          &xy566SEConfigArg0, &xy566SEConfigArg1, &xy566SEConfigArg2, &xy566SEConfigArg3};
static const iocshFuncDef xy566SEConfigFuncDef = 
          {"xy566SEConfig", 4, xy566SEConfigArgs};
static void xy566SEConfigCallFunc(const iocshArgBuf *args)
{
   xy566SEConfig(args[0].ival, args[1].ival,args[2].ival,args[3].ival); 
}


/* make xy566DIConfig() available to the ioc shell */
static const iocshArg xy566DIConfigArg0 = {"number of cards",                iocshArgInt};
static const iocshArg xy566DIConfigArg1 = {"number of channels",             iocshArgInt};
static const iocshArg xy566DIConfigArg2 = {"VME A16 registers base address", iocshArgInt};
static const iocshArg xy566DIConfigArg3 = {"VME A24 memory base address",    iocshArgInt};
static const iocshArg *xy566DIConfigArgs[] = {
          &xy566DIConfigArg0, &xy566DIConfigArg1, &xy566DIConfigArg2, &xy566DIConfigArg3};
static const iocshFuncDef xy566DIConfigFuncDef = 
          {"xy566DIConfig", 4, xy566DIConfigArgs};
static void xy566DIConfigCallFunc(const iocshArgBuf *args)
{
   xy566DIConfig(args[0].ival, args[1].ival,args[2].ival,args[3].ival); 
}


/* make xy566DILConfig() available to the ioc shell */
static const iocshArg xy566DILConfigArg0 = {"number of cards",                iocshArgInt};
static const iocshArg xy566DILConfigArg1 = {"number of channels",             iocshArgInt};
static const iocshArg xy566DILConfigArg2 = {"VME A16 registers base address", iocshArgInt};
static const iocshArg xy566DILConfigArg3 = {"VME A24 memory base address",    iocshArgInt};
static const iocshArg *xy566DILConfigArgs[] = {
          &xy566DILConfigArg0, &xy566DILConfigArg1, &xy566DILConfigArg2, &xy566DILConfigArg3};
static const iocshFuncDef xy566DILConfigFuncDef = 
          {"xy566DILConfig", 4, xy566DILConfigArgs};
static void xy566DILConfigCallFunc(const iocshArgBuf *args)
{
   xy566DILConfig(args[0].ival, args[1].ival,args[2].ival,args[3].ival); 
}


/* make xy566WFConfig() available to the ioc shell */
static const iocshArg xy566WFConfigArg0 = {"number of cards",                iocshArgInt};
static const iocshArg xy566WFConfigArg1 = {"number of channels",             iocshArgInt};
static const iocshArg xy566WFConfigArg2 = {"VME A16 registers base address", iocshArgInt};
static const iocshArg xy566WFConfigArg3 = {"VME A24 memory base address",    iocshArgInt};
static const iocshArg xy566WFConfigArg4 = {"arm address",                    iocshArgInt};
static const iocshArg xy566WFConfigArg5 = {"interrupt vector",               iocshArgInt};
static const iocshArg *xy566WFConfigArgs[] = {
          &xy566WFConfigArg0, &xy566WFConfigArg1, 
          &xy566WFConfigArg2, &xy566WFConfigArg3,
          &xy566WFConfigArg4, &xy566WFConfigArg5};
static const iocshFuncDef xy566WFConfigFuncDef = 
          {"xy566WFConfig", 6, xy566WFConfigArgs};
static void xy566WFConfigCallFunc(const iocshArgBuf *args)
{
   xy566WFConfig(args[0].ival, args[1].ival,args[2].ival,
                 args[3].ival, args[4].ival,args[5].ival); 
}


 static void
 xy566RegisterCommands(void)
 {
     static int firstTime = 1;
     if (firstTime) {
         iocshRegister(&xy566SEConfigFuncDef, xy566SEConfigCallFunc);
         iocshRegister(&xy566DIConfigFuncDef, xy566DIConfigCallFunc);
         iocshRegister(&xy566DILConfigFuncDef,xy566DILConfigCallFunc);
         iocshRegister(&xy566WFConfigFuncDef, xy566WFConfigCallFunc);
         firstTime = 0;
     }
 }
 epicsExportRegistrar(xy566RegisterCommands);
