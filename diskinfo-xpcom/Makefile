Target=Diskinfo
target=diskinfo
TARGET=DISKINFO

CC=gcc
CPP=g++

OBJS=ns$(Target).o

FF3_SDK_INC=/usr/include/firefox-3.6
FF3_SDK_LIB=/usr/lib/firefox-3.6 -lnspr4 -lxpcom -lxpcomglue -lxpcomglue_s
FF3_SDK_IDL=/usr/share/idl/firefox-3.6
XPIDL=/usr/bin/xpidl

DEBUG= -DXP_UNIX -DHAVE_DEBUG -DHAVE_CPP_CHAR16_T

#MOZ_CFLAGS=-DMOZILLA_INTERNAL_API -I $(FF3_SDK_INC) -I $(FF3_SDK_UNSTABLE)
MOZ_CFLAGS= -I $(FF3_SDK_INC) -DMOZILLA_INTERNAL_API #-DHAVE_CPP_CHAR16_T
GTK2_CFLAGS=-Wall `pkg-config  glib-2.0 --cflags`
GTK2_LIBS=`pkg-config glib-2.0 --libs`

CFLAGS= $(DEBUG) $(GTK2_CFLAGS) -I $(FF3_SDK_INC) 
LDFLAGS= -L $(FF3_SDK_LIB) $(GTK2_LIBS)
IDL_CFLAGS=-I $(FF3_SDK_IDL)

TARGET_IDL=nsI$(Target).idl
TARGET_XPT=../lib$(target).xpt
SHAREDLIB=../lib$(target).so

$(SHAREDLIB): $(OBJS)
	$(CPP) --share --soname -o $(SHAREDLIB) $(OBJS) $(LDFLAGS)

ns$(Target).o: nsI$(Target).h ns$(Target).cpp
	$(CPP) -fPIC $(MOZ_CFLAGS) $(GTK2_CFLAGS) -o ns$(Target).o -c ns$(Target).cpp

nsI$(Target).h: $(TARGET_IDL) $(TARGET_XPT)
	$(XPIDL) -m header $(IDL_CFLAGS) -e nsI$(Target).h $(TARGET_IDL)

$(STRIPELIB):$(SHAREDLIB)
	strip $(SHAREDLIB) -o $(STRIPELIB)

$(TARGET_XPT): $(TARGET_IDL)
	$(XPIDL) -m typelib $(IDL_CFLAGS) -e $(TARGET_XPT) $(TARGET_IDL)

clean:
	rm $(OBJS) $(SHAREDLIB) $(TARGET_XPT) -f
