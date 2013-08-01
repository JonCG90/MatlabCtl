MEXSUFFIX  = mexmaci64
MATLABHOME = /Applications/MATLAB_R2012a.app
MEX        = $(MATLABHOME)/bin/mex
CXX        = g++
CFLAGS     = -g -c -fPIC -ansi -pthread -DMX_COMPAT_32 -DMATLAB_MEX_FILE

ILMBASEINC ?= /usr/local/Cellar/ilmbase/1.0.2/include/OpenEXR
ILMBASELIB ?= /usr/local/Cellar/ilmbase/1.0.2/lib
OPENEXR ?= /opt/local
TIFFDIR ?= /usr/local
ACES_CONT ?= /Users/oscar/src/aces_container
CTLINC ?= /Users/oscar/CTL/lib
CTLLIB ?= /Users/oscar/CTL/build/lib
CTLRENDERINC ?= /Users/oscar/CTL/ctlrender
LIBPATH ?= -L$(ILMBASELIB) -L$(CTLLIB)/IlmCtl -L$(CTLLIB)/IlmCtlMath -L$(CTLLIB)/IlmCtlSimd -L$(CTLLIB)/IlmImfCtl -L$(CTLLIB)/dpx -L$(OPENEXR)/lib -L$(TIFFDIR)/lib
LIBS      = $(LIBPATH) -lm -lIlmCtl -lIlmCtlMath -lIlmCtlSimd -lIlmThread -lHalf -lIex -lctldpx -lIlmImfCtl -lIlmImf -ltiff -lAcesContainer
INCLUDE   = -I$(MATLABHOME)/extern/include -I$(CTLINC)/IlmCtl -I$(CTLINC)/IlmCtlSimd -I$(CTLINC)/IlmCtlMath -I$(CTLINC)/IlmImfCtl -I$(CTLINC)/dpx -I$(ILMBASEINC) -I$(CTLRENDERINC) -I$(OPENEXR)/include/OpenEXR -I$(TIFFDIR)/include -I$(ACES_CONT)
MEXFLAGS  = -cxx CC='$(CXX)' CXX='$(CXX)' LD='$(CXX)'	

ctl.$(MEXSUFFIX): CtlMatlab.o transform.cc.o compression.cc.o format.cc.o aces_file.cc.o dpx_file.cc.o exr_file.cc.o usage.cc.o tiff_file.cc.o
	$(MEX) $(MEXFLAGS) $(LIBS) -o ctl.$(MEXSUFFIX) transform.cc.o CtlMatlab.o compression.cc.o format.cc.o aces_file.cc.o dpx_file.cc.o exr_file.cc.o usage.cc.o tiff_file.cc.o

CtlMatlab.o: CtlMatlab.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) -o CtlMatlab.o CtlMatlab.cpp
    
#compression.cc.o: compression.cc compression.hh
#    $(CXX) $(CFAGS) $(INCLUDE) -o compression.cc.o compression.cc

clean:
	rm -rf *.o *.os *.mexmaci64