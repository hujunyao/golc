Target=STUtils
target=stutils
TARGET=STUTILS

CC=cl
CPP=cl

SRC=ns$(Target).cpp

FF3_SDK_LIB=/usr/lib/firefox-3.6.6
FF3_SDK_IDL="C:\xulrunner-sdk\idl"
XPIDL=xpidl.exe

FF3_LIBS=-L $(FF3_SDK_LIB) -lnspr4 -lxpcom -lxpcomglue -lxpcomglue_s

CFLAGS= /I"C:\xulrunner-sdk\include"
LDFLAGS= $(FF3_LIBS) $(GTK2_LIBS)
IDL_CFLAGS=-I $(FF3_SDK_IDL)
WIN32_CFLAGS=/nologo /MT /LD /O1 /D XP_WIN /D XP_WIN32 /Foobj /I"C:\vc2005\PlatformSDK\Include" /I"C:\vc2005\include"
WIN32_LDFLAGS=/link /LIBPATH:"C:\vc2005\PlatformSDK\lib" /LIBPATH:"C:\vc2005\lib"  stutils.RES kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /LIBPATH:"C:\xulrunner-sdk\lib" xpcomglue_s.lib xpcom.lib nspr4.lib plc4.lib /nologo /dll /machine:I386

TARGET_IDL=nsI$(Target).idl
TARGET_XPT=../lib$(target).xpt
SHAREDLIB=../lib$(target).dll

$(SHAREDLIB): $(SRC) nsI$(Target).h $(target).RES
	$(CPP) $(CFLAGS) $(SRC) $(WIN32_CFLAGS) $(WIN32_LDFLAGS) /out:$(SHAREDLIB)

$(target).RES: $(target).rc
	rc.exe /i "C:\vc2005\PlatformSDK\Include" /i "C:\vc2005\include" $(target).rc
nsI$(Target).h: $(TARGET_IDL) $(TARGET_XPT)
	$(XPIDL) -m header $(IDL_CFLAGS) -e nsI$(Target).h $(TARGET_IDL)

$(TARGET_XPT): $(TARGET_IDL)
	$(XPIDL) -m typelib $(IDL_CFLAGS) -e $(TARGET_XPT) $(TARGET_IDL)

clean:
	rm ../libstutils* *.obj *.exp *.lib nsISTUtils.h *.RES
