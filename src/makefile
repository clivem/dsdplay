CC=gcc
ARCH = $(shell echo $$HOSTTYPE)
CFLAGS=-W -Wall -Wstrict-prototypes -O3 -fomit-frame-pointer -pipe
CFLAGS=-g -W -Wall -Wstrict-prototypes
DEPS = 
OBJ = 
OBJ = $(ARCH)/obj

OBJS = $(OBJ)/dsdplay.o $(OBJ)/dsdinput.o $(OBJ)/dsf.o $(OBJ)/dsdiff.o $(OBJ)/dsd2pcm.o $(OBJ)/dsdoutput.o

BIN = $(ARCH)/dsdplay

GLIB = $(shell pkg-config --libs glib-2.0)
GLIBINC = $(shell pkg-config --cflags glib-2.0)

all: $(OBJ) $(BIN)

$(OBJ)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(GLIBINC)

$(OBJ)/%.o: libdsd/%.c
	$(CC) -c -o $@ $< $(CFLAGS) $(GLIBINC)

$(OBJ)/%.o: dsd2pcm/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ):
	mkdir -p $(OBJ)

$(ARCH)/dsdplay: $(OBJS)
	gcc -o $@ $^ $(CFLAGS) $(GLIB)

clean:
	rm -rf $(ARCH)
	find . -name \*~ -exec rm {} \;
