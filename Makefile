ADBFUSE_BASE := /home/nicolas/workspace/C/adbfuse/
ADB_BASE := $(ADBFUSE_BASE)/upstream/core/
ADB_SRC := \
	adb.c \
	console.c \
	transport.c \
	transport_local.c \
	transport_usb.c \
	commandline.c \
	adb_client.c \
	adb_auth_host.c \
	sockets.c \
	services.c \
	fdevent.c \
	file_sync_client.c \
	utils.c \
	usb_vendors.c \
	usb_linux.c \
	get_my_path_linux.c
ZIPFILE_SRC := \
	zipfile.c \
	centraldir.c
CUTILS_SRC := \
	socket_network_client.c \
	socket_loopback_client.c \
	socket_local_client.c \
	socket_loopback_server.c \
	socket_local_server.c \
	socket_inaddr_any_server.c \
	list.c \
	load_file.c
SRC := df_host.c \
       df_protocol.c \
       df_io.c

SRC += $(ADB_SRC)
SRC += $(ZIPFILE_SRC)
SRC += $(CUTILS_SRC)

CC ?= gcc
OBJ = $(SRC:.c=.o)
BIN = adbfuse
CFLAGS += `pkg-config fuse --cflags` -Wall -O0 -g #-Wextra #-Werror
CFLAGS += -I$(ADB_BASE)/adb/ -I$(ADB_BASE)/include/
CFLAGS += -DADB_HOST=1
CFLAGS += -D_XOPEN_SOURCE -D_GNU_SOURCE
CFLAGS += -DHAVE_FORKEXEC -DHAVE_TERMIO_H
LDFLAGS += `pkg-config fuse --libs` -pthread -lrt -lncurses -lpthread -lcrypto

VPATH := $(ADBFUSE_BASE)/src/
VPATH += $(VPATH):$(ADB_BASE)/adb
VPATH += $(VPATH):$(ADB_BASE)/libcutils
VPATH += $(VPATH):$(ADB_BASE)/libzipfile

all:$(BIN)

.PHONY:clean mrproper

$(BIN):$(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ)

mrproper:clean
	rm -f $(BIN)
