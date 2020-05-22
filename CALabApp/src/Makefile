TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

SRC_DIRS += $(TOP)/caLab_src/src

LIBRARY_IOC += caLab caLabIoc

ifeq (WIN32,$(OS_CLASS))
ifneq ($(findstring windows,$(EPICS_HOST_ARCH)),)
LABVIEWDIR=$(subst /,\,$(ICPBINARYDIR)/LabVIEW/2010_x64)
else
LABVIEWDIR=$(subst /,\,$(ICPBINARYDIR)/LabVIEW/2016_x86)
endif
else
LABVIEWDIR=$(subst /,\,$(ICPBINARYDIR)/LabVIEW/2016_x86)
endif

USR_CXXFLAGS_WIN32 += -DCALAB_EXPORTS -DWIN32 -I$(LABVIEWDIR)/cintools

# specify all source files to be compiled and added to the library
caLab_SRCS += caLab.cpp
caLabIoc_SRCS += caLabIoc.cpp

#caLab_RCS_WIN32 += caLab.rc   # missing .ico files

USR_LDFLAGS_WIN32 += /LIBPATH:$(LABVIEWDIR)/cintools

caLab_LIBS += $(EPICS_BASE_IOC_LIBS)

caLab_SYS_LIBS_WIN32 += labview user32
# these are needed for DLL build on static EPICS target
caLab_SYS_LIBS_WIN32 += ws2_32 dbghelp advapi32

#===========================

# build a shared library on a static build
ifeq ($(STATIC_BUILD),YES)
BIN_INSTALLS += $(SHRLIB_PREFIX)caLab$(SHRLIB_SUFFIX) $(SHRLIB_PREFIX)caLabIoc$(SHRLIB_SUFFIX)
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# special rule to build a DLL from a static compile, taken from standard EPICS DLL build rules.
# it builds the DLL, but the as the DLL build creates an import library that overwrites the static library 
# prviously made. we don't need the static or import library here, but create the import library 
# under another name for completeness
ifeq ($(STATIC_BUILD),YES)
$(SHRLIB_PREFIX)caLab$(SHRLIB_SUFFIX) : $(SHRLIB_PREFIX)%$(SHRLIB_SUFFIX): $(LIB_PREFIX)%$(LIB_SUFFIX)
	@$(RM) $@
ifeq ($(SHRLIB_SUFFIX),.dll)
	$(LINK.shrlib) /implib:caLab_dll.lib
	$(MT_DLL_COMMAND)
else
	$(LINK.shrlib)
endif
$(SHRLIB_PREFIX)caLabIoc$(SHRLIB_SUFFIX) : $(SHRLIB_PREFIX)%$(SHRLIB_SUFFIX): $(LIB_PREFIX)%$(LIB_SUFFIX)
	@$(RM) $@
ifeq ($(SHRLIB_SUFFIX),.dll)
	$(LINK.shrlib) /implib:caLabIoc_dll.lib
	$(MT_DLL_COMMAND)
else
	$(LINK.shrlib)
endif
endif
