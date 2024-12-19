CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -lpthread -lrt
TARGET ?= aesdsocket

ROOT_DIR=.

SRC_FILES=\
  $(ROOT_DIR)/aesdsocket.c 

INC_DIRS=-I$(ROOT_DIR)/

all: aesdsocket_all

default: aesdsocket_default

aesdsocket_all:
	$(CC) $(CFLAGS) $(INC_DIRS) $(SRC_FILES) -o $(TARGET) -v

clean:
	rm -f $(TARGET) *.o

