PRJ_NAME=focuser
CC=gcc
LINKER=g++
CPPFLAGS=-I./inc
CFLAGS=-O2 -Wall
LDFLAGS=-L./libs -liup -liupcontrols -liup_pplot -liupcd -lcd -lcdcontextplus -lim `pkg-config --libs gtk+-2.0` -lX11 -lz

VPATH=.

SRCS := $(shell ls *.c)
OBJS := $(SRCS:.c=.o)

$(PRJ_NAME):$(OBJS)
	$(LINKER) $^ $(LDFLAGS) -o $@

.PHONY: clean
clean:
	rm -f *.o
	rm -f $(PRJ_NAME)
