### Options

# The following options are recognized:
#
#   NO_GHOSTSCRIPT   don't include GhostScript support
#

GG_PATH       = /boot/apps/GeekGadgets
LIBPREFS_PATH = ../libprefs125/src
LIBPROF_PATH  = /boot/develop/BeIDE/profiling/libprof/

ifeq '$(BE_HOST_CPU)' 'ppc'

  CC            = mwcc -I. -I- -msg nodup
  CXX           = $(CC) -dialect cplus
  OPTFLAGS      = -w all,nopossible,noimplicitconv,nonotinlined -opt full
  DEBUGFLAGS    = -g -map $@.xMAP #-DDEBUG
  PROFFLAGS     = #-profile on -DPROFILING -L$(LIBPROF_PATH) -I$(LIBPROF_PATH) -lprof
  HANDLER_FLAGS = -export pragma
  LDLIBS        = -lkpathsea -lgs -lprefs -ltracker -ltranslation

else

  CC            = gcc
  CXX           = $(CC)
  OPTFLAGS      = -Wall -Wno-multichar -Wno-reorder -Wno-ctor-dtor-privacy -Wno-sign-compare -O2
  DEBUGFLAGS    = #-g -DDEBUG
  PROFFLAGS     = #-DPROFILING -L$(LIBPROF_PATH) -I$(LIBPROF_PATH) -lprof
  HANDLER_FLAGS =
  LDLIBS        = -lkpathsea -lgs -lprefs -ltracker -ltranslation -lbe -lroot -lstdc++.r4

endif

INCLUDEFLAGS  = -I$(GG_PATH)/include -I$(LIBPREFS_PATH)
CFLAGS        = $(OPTFLAGS) $(DEBUGFLAGS) $(PROFFLAGS) $(INCLUDEFLAGS) $(XCFLAGS)
CXXFLAGS      = $(CFLAGS) $(XCXXFLAGS)
LDFLAGS       = -L$(GG_PATH)/lib -L$(HOME)/config/lib $(DEBUGFLAGS) $(PROFFLAGS)

.PHONY: all clean localize

### BeDVI

all: BeDVI DVIHandler

BeDVI: BeDVI.o DVI-Window.o DVI-View.o DVI.o DVI-DrawPage.o DVI-Special.o GhostScript.o MeasureWin.o SearchWin.o \
       FontList.o TeXFont.o PK.o GF.o VF.o Support.o DocView.o log.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
	xres -o BeDVI BeDVI.rsrc
	mwbres -merge -o BeDVI BeDVI.r
	mimeset -f BeDVI

DVIHandler: DVIHandler.o DVI.o DVI-DrawPage.o DVI-Special.o GhostScript.o FontList.o TeXFont.o PK.o GF.o VF.o Support.o \
            log.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@ $(HANDLER_FLAGS)


BeDVI.o:         BeDVI.cc DVI-View.h FontList.h defines.h BeDVI.h DVI.h DocView.h
DVI.o:           DVI.cc DVI.h DVI-DrawPage.h defines.h FontList.h BeDVI.h DVI-View.h TeXFont.h DocView.h
DVI-DrawPage.o:  DVI-DrawPage.cc DVI.h DVI-DrawPage.h TeXFont.h
DVI-Special.o:   DVI-Special.cc DVI.h DVI-DrawPage.h defines.h BeDVI.h
DVI-Window.o:    DVI-Window.cc defines.h BeDVI.h DVI-View.h DVI.h FontList.h DocView.h
DVI-View.o:      DVI-View.cc DVI-View.h DVI.h defines.h BeDVI.h TeXFont.h FontList.h DocView.h
DVIHandler.o:    DVIHandler.cc DVI.h BeDVI.h defines.h
FontList.o:      FontList.cc FontList.h TeXFont.h defines.h BeDVI.h DVI.h
GhostScript.o:   GhostScript.cc DVI.h DVI-DrawPage.h PSHeader.h
MeasureWin.o:    MeasureWin.cc BeDVI.h
SearchWin.o:     SearchWin.cc BeDVI.h
Support.o:       Support.cc Support.h
TeXFont.o:       TeXFont.cc TeXFont.h defines.h BeDVI.h DVI-View.h DVI.h DVI-DrawPage.h FontList.h DocView.h
PK.o:            PK.cc TeXFont.h defines.h BeDVI.h
GF.o:            GF.cc TeXFont.h defines.h BeDVI.h
VF.o:            VF.cc TeXFont.h defines.h BeDVI.h FontList.h DVI.h DVI-View.h DocView.h
DocView.o:       DocView.cc DocView.h
log.o:           log.cc log.h

### PS Header

PSHeader.h: PSHeader.ps squeeze
	./squeeze PSHeader.ps $@

squeeze:   squeeze.o
squeeze.o: squeeze.c

### misc

localize:
	mwbres -merge -o BeDVI BeDVI.r

clean:
	rm -f *.o *.xSYM *.xMAP squeeze PSHeader.h
