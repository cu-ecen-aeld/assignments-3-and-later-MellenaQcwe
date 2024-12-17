C_COMPILER=$(CROSS_COMPILE)$(CC)

ifeq ($(shell uname -s), DontUseClang)
	C_COMPILER=clang
endif

CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Werror
ifeq ($(DEBUG), 1)
	CFLAGS += -g
endif

ROOT_DIR=.

TARGET=aesdsocket

SRC_FILES=\
  $(ROOT_DIR)/aesdsocket.c 

INC_DIRS=-I$(ROOT_DIR)/

all: aesdsocket_all

default: aesdsocket_default

aesdsocket_all:
	$(C_COMPILER) $(CFLAGS) $(INC_DIRS) $(SRC_FILES) -o $(TARGET) -v

clean:
	rm -f $(TARGET) *.o

