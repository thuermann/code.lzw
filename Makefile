#
# $Id: Makefile,v 1.1 2015/02/18 23:34:44 urs Exp $
#

RM = rm -f

programs = lzw

.PHONY: all
all: $(programs)

LZW_OBJ = main.o lzw.o
lzw: $(LZW_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(LZW_OBJ) $(LDLIBS)

lzw.o main.o: lzw.h

.PHONY: clean
clean:
	$(RM) $(programs) *.o core
