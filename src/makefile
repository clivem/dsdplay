CC      ?= gcc
CFLAGS  ?= -W -Wall -Wstrict-prototypes -O3
LDFLAGS ?= -lFLAC -lsoxr

BUILD_DIR = build

OBJS = $(BUILD_DIR)/dsdplay.o \
       $(BUILD_DIR)/dsdinput.o \
       $(BUILD_DIR)/dsf.o \
       $(BUILD_DIR)/dsdiff.o \
       $(BUILD_DIR)/dsd2pcm.o \
       $(BUILD_DIR)/dsdoutput.o

BIN = $(BUILD_DIR)/dsdplay

all: $(BUILD_DIR) $(BIN)

$(BUILD_DIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(BUILD_DIR)/%.o: libdsd/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(BUILD_DIR)/%.o: dsd2pcm/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN): $(OBJS)
	gcc -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)
	find . -name \*~ -exec rm {} \;
