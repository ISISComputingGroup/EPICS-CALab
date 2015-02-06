TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

SRC_DIRS += $(TOP)/caLab_1505/src

LIBRARY_IOC += CALab

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install CALab.dbd into <top>/dbd
#DBD += CALab.dbd

USR_CXXFLAGS += -DCALAB_EXPORTS -DWIN32 -I$(LABVIEWDIR)/cintools

# specify all source files to be compiled and added to the library
CALab_SRCS += caLab.cpp

#CALab_RCS += caLab.rc   # missing .ico files

USR_LDFLAGS_WIN32 += /LIBPATH:$(LABVIEWDIR)/cintools

CALab_LIBS += $(EPICS_BASE_IOC_LIBS)

CALab_SYS_LIBS_WIN32 += labview user32

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

