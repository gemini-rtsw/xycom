TOP=../..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += xy566
xy566_SRCS += devAiXy566Di.c
xy566_SRCS += devAiXy566DiL.c
xy566_SRCS += devAiXy566Se.c
xy566_SRCS += devWfXy566Sc.c
xy566_SRCS += drvXy566.c

DBD += xy566.dbd
DBD += xy566Supp.dbd

xy566Supp_DBD += base.dbd
xy566Supp_DBD += xy566driver.dbd

xy566_DBD += $(xy566Supp_DBD)

include $(TOP)/configure/RULES

