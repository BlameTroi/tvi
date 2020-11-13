# makefile for tvi

CC = gcc
CFLAGS = -g -Wall -Wextra -pedantic -std=c99
# LFLAGS = -L...
LFLAGS = -g
# LIBS = -l... -lm
LIBS =
INCLUDES =

SRCS = tvi.c highlight.c terminal.c rowscreen.c
OBJS = $(SRCS:.c=.o)
HDRS = $(SRCS:.c=.h)

MAIN = tvi

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) -o $(MAIN) $(LFLAGS) $(LIBS) $(OBJS)

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $(INCLUDES) $<

clean:
	$(RM) $(OBJS) $(MAIN)
