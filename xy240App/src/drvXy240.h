/*
 * Header file for drvXy240.c
 */

#define XY240_BI 0
#define OK 0
#define ERROR (-1)

long xy240_init();
long xy240_getioscanpvt(short card, IOSCANPVT *scanpvt);
long xy240_bi_driver(short card, epicsUInt32 mask, epicsUInt32 *prval);
long xy240_bo_read(short card, epicsUInt32 mask, epicsUInt32 *prval);
long xy240_bo_driver(short card, epicsUInt32 val, epicsUInt32 mask);
long xy240_io_report(int level);
