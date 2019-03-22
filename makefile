MGLINCLUDE=i:/mgl/include

all: mglemx.exe

mglemx.exe: mglemx
	emxbind -bf mglemx

mglemx: mgltest.o
	gcc -Zcrtdll -Zmt -fpack-struct -o mglemx mgltest.o -lmgl -lcommon -lfreetype -lpm

mgltest.o: mgltest.c
	gcc -Wall -Wno-char-subscripts -Zcrtdll -Zmt -fpack-struct -D__OS2__=1 -D__OS2_CONSOLE__ -c -I$(MGLINCLUDE) -o mgltest.o mgltest.c

