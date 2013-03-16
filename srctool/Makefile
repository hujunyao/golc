CC=gcc
TARGET=jtool

BASESRC=base.c
JLEXOBJS=java-lex.c 
JTOOL=jtool.c

GLIB_CFLAGS=`pkg-config glib-2.0 --cflags`
GLIB_LIBS=`pkg-config glib-2.0 --libs`

all: $(JTOOL) $(JLEXOBJS) $(BASESRC)
	$(CC) -o $(TARGET)  $(JTOOL) $(JLEXOBJS) $(BASESRC) $(GLIB_CFLAGS) $(GLIB_LIBS)

$(JLEXOBJS): java-lex.l
	flex java-lex.l
clean:
	rm java-lex.c java-lex.h $(TARGET)
