CC=gcc
CFLAGS=-I$(IDIR) -g -ggdb

ODIR=./
#LDIR =../lib

LIBS=-lm -lasound -ljson-c

#_DEPS = hellomake.h
#DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = midil.o midil.o 
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

midil: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: all clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
