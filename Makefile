#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure
DIRS += xy240
DIRS += xy566
include $(TOP)/configure/RULES_TOP
