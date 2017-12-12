/*
 * Header file for drvXy240.c
 */

#ifndef DRV_XY240_H
#define DRV_XY240_H

#define OK 0
#define ERROR (-1)

/*
 * Definitions of the bits in the control & status register.
 */
#define XY240_CSR_RED_LED_OFF       0x01    /* 1=off 0=on */
#define XY240_CSR_GREEN_LED_ON      0x02    /* 1=on 0=off */
#define XY240_CSR_INT_PENDING       0x04    /* read-only bit indictaes if interrupts are pending */
#define XY240_CSR_INT_ENA           0x08    /* global card interrupt enable bit */
#define XY240_CSR_SOFTWARE_RESET    0x10    /* 0,1,0 causes board reset */
#define XY240_CSR_USER_BIT_0        0x20
#define XY240_CSR_USER_BIT_1        0x40    /* three bits for user */
#define XY240_CSR_USER_BIT_2        0x80

#define XY240_RED_LED     0  
#define XY240_GREEN_LED   1

long xy240_init();
long xy240_getioscanpvt(short card, IOSCANPVT *scanpvt);
long xy240_bi_driver(short card, epicsUInt32 mask, epicsUInt32 *prval);
long xy240_bo_read(short card, epicsUInt32 mask, epicsUInt32 *prval);
long xy240_bo_driver(short card, epicsUInt32 val, epicsUInt32 mask);
long xy240_io_report(int level);

unsigned long xy240_status(int cardnum);
long xy240_intConnect(int cardnum, int irqchan, void (*proutine)(int));
long xy240_intDisconnect(int cardnum, int irqchan);
int xy240_writeIMR(int cardnum, epicsUInt8 byteval);
int  xy240_readIPR(int cardnum);
int xy240_writePortBit(int cardnum, epicsUInt8 portnum, epicsUInt8 bitnum, epicsBoolean bitval);
int xy240_writePortByte(int cardnum, epicsUInt8 portnum, epicsUInt8 byteval);
int xy240_writeFlagBit(int cardnum, epicsUInt8 bitnum, epicsBoolean bitval);
int xy240_setResetFlagBits(int cardnum, epicsUInt8 bitmask, epicsBoolean set);
int xy240_writeFlagByte(int cardnum, epicsUInt8 bitnum, epicsUInt8 byteval);
int xy240_writeCSRBit(int cardnum, epicsUInt8 bitnum, epicsBoolean bitval);
epicsInt16 xy240_readPortByte(int cardnum, epicsUInt8 portnum);
int xy240_ledCtl(int cardnum, epicsUInt8 led, epicsBoolean val);


#endif
