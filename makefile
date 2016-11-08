INCLUDES = -I/mnt/proj/software/k2/Arnold-4.2.10.0-linux/include
LIBS = -L/mnt/proj/software/k2/Arnold-4.2.10.0-linux/bin


fuse.so : fuse.o FuseNormal.o FuseShading.o alUtil.o
	g++ -o $@ -shared $(LIBS) -lai $<
alUtil.o : /mnt/public/home/john/arnoldshader_src/alShaders-src-1.0.0rc11/common/alUtil.cpp alUtil.h
	g++ -o $@ -fPIC -O2 -c $(INCLUDES) $<
fuse.o : fuse.cpp fuse.h alUtil.o
	g++ -o $@ -fPIC -O2 -c $(INCLUDES) $<
FuseNormal.o : FuseNormal.cpp fuse.h alUtil.o
	g++ -o $@ -fPIC -O2 -c $(INCLUDES) $<
FuseShading.o : FuseShading.cpp fuse.h alUtil.o
	g++ -o $@ -fPIC -O2 -c $(INCLUDES) $<

