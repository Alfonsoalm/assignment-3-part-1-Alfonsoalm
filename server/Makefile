
# server/Makefile – versión compatible con Buildroot y AArch64

# El prefijo CROSS_COMPILE lo inyecta Buildroot en $(TARGET_CONFIGURE_OPTS)
# pero añadimos un fallback por seguridad
CROSS_COMPILE ?= aarch64-buildroot-linux-gnu-
CC            ?= $(CROSS_COMPILE)gcc

CFLAGS  ?= -Wall -Wextra -O2 \
           -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS ?=

TARGET = aesdsocket
SRCS   = aesdsocket.c
OBJS   = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
    $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -f $(OBJS) $(TARGET)
