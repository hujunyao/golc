COMMSRC=common/base.c \
				common/inet.c \
				common/ipc.c 

GWAPPSRC=app/blist.c \
				 app/config.c \
				 app/session.c \
			   app/gw-app.c

GWZNETSRC=znet/znet.c \
					znet/gw-znet.c \
					protoeng/xmlbuf.c

GWPROTOSRC=protoeng/protoeng.c \
					 protoeng/gw-proto.c \
					 protoeng/xmlbuf.c \
					 protoeng/xml2zcmd.c \
					 protoeng/zcmd2xml.c \
					 protoeng/pem_zcmd.c \
					 protoeng/pem_log.c

CFLAGS=`pkg-config libevent libxml-2.0 --cflags` -Icommon
LIBS=`pkg-config libevent libxml-2.0 --libs`

SQLITES_CFLAGS=`pkg-config sqlite3 --cflags`
SQLITES_LIBS=`pkg-config sqlite3 --libs`

app: $(COMMSRC) $(GWAPPSRC)
	arm-arago-linux-gnueabi-gcc -o bin/gw.app $(COMMSRC) $(GWAPPSRC) $(CFLAGS) $(LIBS) -Iapp/

zzz: $(COMMSRC) $(GWZNETSRC)
	arm-arago-linux-gnueabi-gcc -o bin/gw.znet $(COMMSRC) $(GWZNETSRC) $(CFLAGS) $(LIBS) $(SQLITES_CFLAGS) $(SQLITES_LIBS) -Iznet/

proto: $(COMMSRC) $(GWPROTOSRC)
	arm-arago-linux-gnueabi-gcc -g -o bin/gw.proto $(COMMSRC) $(GWPROTOSRC) $(CFLAGS) $(LIBS) -Iprotoeng/
test:
	arm-arago-linux-gnueabi-gcc -o bin/gw.xmlbuf.test protoeng/xmlbuf.c protoeng/xmlbuf-test.c $(CFLAGS) $(LIBS)
	arm-arago-linux-gnueabi-gcc -o bin/gw.proto.test protoeng/gw-ipc-test.c common/ipc.c common/base.c $(CFLAGS) $(LIBS)

clean:
	rm common/*.o app/*o znet/*.o protoeng/*.o
