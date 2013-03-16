Target=CNUtils
target=cnutils
TARGET=CNUTILS

CC=cl
CPP=cl

SRC=ns$(Target).cpp

FF3_SDK_IDL="C:\xulrunner-sdk\idl"
XPIDL=xpidl.exe

CFLAGS= /I"C:\xulrunner-sdk\include"
IDL_CFLAGS=-I $(FF3_SDK_IDL)
WIN32_CFLAGS=/nologo /MT /LD /O1 /D MOZ_NO_MOZALLOC /D XP_WIN /D XP_WIN32 /Foobj /I"C:\Program Files\Microsoft Visual Studio 10.0\VC\include" /I"C:\Program Files\Microsoft SDKs\Windows\v7.0A\Include"
WIN32_LDFLAGS=/link /LIBPATH:"C:\Program Files\Microsoft Visual Studio 10.0\VC\lib" /LIBPATH:"C:\Program Files\Microsoft SDKs\Windows\v7.0A\lib"  cnutils.RES kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comsuppw.lib /LIBPATH:"C:\xulrunner-sdk\lib" xpcomglue_s_nomozalloc.lib xpcom.lib nspr4.lib plc4.lib /nologo /dll /machine:I386

TARGET_IDL=nsI$(Target).idl
TARGET_XPT=../lib$(target).xpt
SHAREDLIB=../lib$(target).dll

$(SHAREDLIB): $(SRC) nsI$(Target).h $(target).RES
	$(CPP) $(CFLAGS) $(SRC) $(WIN32_CFLAGS) $(WIN32_LDFLAGS) /out:$(SHAREDLIB)

$(target).RES: $(target).rc
	rc.exe /i "C:\Program Files\Microsoft SDKs\Windows\v7.0A\Include" /i "C:\Program Files\Microsoft Visual Studio 10.0\VC\include" $(target).rc
nsI$(Target).h: $(TARGET_IDL) $(TARGET_XPT)
	$(XPIDL) -m header $(IDL_CFLAGS) -e nsI$(Target).h $(TARGET_IDL)

$(TARGET_XPT): $(TARGET_IDL)
	$(XPIDL) -m typelib $(IDL_CFLAGS) -e $(TARGET_XPT) $(TARGET_IDL)

clean:
	rm ../libcnutils* *.obj *.exp *.lib nsICNUtils.h *.res
