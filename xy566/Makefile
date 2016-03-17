TOP=..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += xy566
DBD = xy566.dbd
xy566_LIBS += $(EPICS_BASE_IOC_LIBS)
xy566_SRCS += devAiXy566Di.c
xy566_SRCS += devAiXy566DiL.c
xy566_SRCS += devAiXy566Se.c
xy566_SRCS += devWfXy566Sc.c
xy566_SRCS += drvXy566.c

include $(TOP)/configure/RULES

