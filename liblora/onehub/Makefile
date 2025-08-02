### get external defined data

LIBSX126X_VERSION := `cat VERSION`

### constant symbols

ARCH ?=
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

INC := -I../ -Ionehub/lorahub -Iradio_drivers -Iradio_drivers/sx126x_driver/src -Ismtc_ral/bsp/sx126x/ -Ismtc_ral/src
LCFLAGS := $(CFLAGS) $(LDEF) -fPIC $(INC)
CFLAGS_SO := -shared -O2 -Wall -DLINUX_SO

SRC_DIRS := lorahub radio_drivers smtc_ral
OBJ_DIR := obj

INCLUDES = $(shell find . -type f -name '*.h') ../basic_aux.h ../basic_hal.h
SRCS = $(foreach dir,$(SRC_DIRS),$(shell find $(dir) -type f -name '*.c'))
OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))


### linking options

#LIBS := -lsx126xhal -Llorahub -lm

### general build targets

all: 	libsx126xhal.so 

clean:
	rm -f libsx126xhal.so
	rm -f $(OBJ_DIR)/??*

### transpose library.cfg into a C header file : config.h

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) -c $(LCFLAGS) $< -o $@

### dynimic library

libsx126xhal.so: $(OBJS)
	$(CC) $(CFLAGS_SO) -o $@ $^

### EOF
