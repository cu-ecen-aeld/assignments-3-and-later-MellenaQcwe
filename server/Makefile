CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -lpthread -lrt
TARGET ?= aesdsocket

# Switch logging to either char dev (/dev/aesdchar) or regular file (/var/tmp/aesdsocketdata) 
USE_AESD_CHAR_DEVICE = y
ifeq ($(USE_AESD_CHAR_DEVICE),y)
EXTRA_CFLAGS += -DUSE_AESD_CHAR_DEVICE
endif

ROOT_DIR=.

SRC_FILES=\
  $(ROOT_DIR)/aesdsocket.c 

INC_DIRS=-I$(ROOT_DIR)/

all: aesdsocket_all

default: aesdsocket_default

aesdsocket_all:
	$(CC) $(EXTRA_CFLAGS) $(CFLAGS) $(INC_DIRS) $(SRC_FILES) -o $(TARGET) -v

clean:
	rm -f $(TARGET) *.o

