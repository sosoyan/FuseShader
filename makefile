INCLUDES = -I$(ARNOLD_ROOT)/include
LIBS = -L$(ARNOLD_ROOT)/bin

fuse.dylib : fuse.o FuseNormal.o FuseShading.o alUtil.o
	g++ -o $@ -shared $(LIBS) -lai $<
alUtil.o : alUtil.cpp alUtil.h
	g++ -o $@ -fPIC -O2 -c $(INCLUDES) $<
fuse.o : fuse.cpp fuse.h alUtil.o
	g++ -o $@ -fPIC -O2 -c $(INCLUDES) $<
FuseNormal.o : FuseNormal.cpp fuse.h alUtil.o
	g++ -o $@ -fPIC -O2 -c $(INCLUDES) $<
FuseShading.o : FuseShading.cpp fuse.h alUtil.o
	g++ -o $@ -fPIC -O2 -c $(INCLUDES) $<

