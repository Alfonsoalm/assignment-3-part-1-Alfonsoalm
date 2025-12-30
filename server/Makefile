
# server/Makefile
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?=
# Librerías para syslog y sockets no necesitan LDFLAGS extra en Linux estándar

TARGET := aesdsocket
SRCS := aesdsocket.c
OBJS := $(SRCS:.c=.o)

.PHONY: all default clean

default: all
all: $(TARGET)

$(TARGET): $(OBJS)
    $(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -f $(OBJS) $(TARGET)
