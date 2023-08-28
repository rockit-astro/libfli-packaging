################################################################################
#  Note: This is a modified Makefile replacement for compiling the 
#        FLI SDK v1.104 as a shared object (.so) library on linux. 
#        The source code can be found at 
#        http://www.flicamera.com/software/index.html
################################################################################
RPMBUILD = rpmbuild --define "_topdir %(pwd)/build" \
        --define "_builddir %{_topdir}" \
        --define "_rpmdir %{_topdir}" \
        --define "_srcrpmdir %{_topdir}" \
        --define "_sourcedir %(pwd)"


DIR	= $(shell pwd)
CC	= gcc
INC	= $(DIR)
CFLAGS	= -fPIC -Wall -O2 -g $(patsubst %, -I%, $(INC))
AR	= ar
ARFLAGS	= -rus

SYS	= libfli-sys.o
DEBUG	= libfli-debug.o
MEM	= libfli-mem.o
USB_IO	= libfli-usb.o libfli-usb-sys.o
PPORT_IO	= libfli-parport.o
SERIAL_IO	= libfli-serial.o
IO	= $(USB_IO) $(PPORT_IO) $(SERIAL_IO)
CAM	= libfli-camera.o libfli-camera-parport.o libfli-camera-usb.o
FILT	= libfli-filter-focuser.o

ALLOBJ	= $(SYS) $(DEBUG) $(MEM) $(IO) $(CAM) $(FILT)

libfli.so: libfli.o $(ALLOBJ)
	$(CC) -shared -o $@ $^

package: libfli.so
	mkdir -p build
	${RPMBUILD} -ba libfli.spec
	mv build/x86_64/*.rpm .
	rm -rf build

install: libfli.so
	cp libfli.so /usr/local/lib

.PHONY: clean
clean:
	rm -f $(ALLOBJ) libfli.o libfli.a
