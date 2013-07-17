MEXSUFFIX  = mexmaci64
MATLABHOME = /Applications/MATLAB_R2013a.app
MEX        = $(MATLABHOME)/bin/mex
CXX        = g++
CFLAGS     = -fPIC -ansi -pthread -DMX_COMPAT_32 -DMATLAB_MEX_FILE

LIBS      = -lm
INCLUDE   = -I$(MATLABHOME)/extern/include
MEXFLAGS  = -cxx CC='$(CXX)' CXX='$(CXX)' LD='$(CXX)'

normpdf.$(MEXSUFFIX): normpdf.o
	$(MEX) $(MEXFLAGS) $(LIBS) -output normpdf $^

normpdf.o: normpdf.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) -c $^
