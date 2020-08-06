TOP=../..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += xy240

INC += drvXy240.h

DBD += xy240.dbd
DBD += xy240Supp.dbd

xy240Supp_DBD += xy240driver.dbd

xy240_DBD += $(xy240Supp_DBD)
xy240_DBD += base.dbd 


xy240_SRCS += drvXy240.c
xy240_SRCS += devXy240.c

include $(TOP)/configure/RULES

