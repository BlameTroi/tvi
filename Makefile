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

# $(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

MAIN = tvi

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) -o $(MAIN) $(LFLAGS) $(LIBS) $(OBJS)

tvi.o: $(SRCS) $(HDRS)
	$(CC) -c $(CFLAGS) $(INCLUDES) tvi.c

highlight.o: highlight.c highlight.h tvi.h
	$(CC) -c $(CFLAGS) $(INCLUDES) highlight.c

terminal.o: terminal.c terminal.h tvi.h
	$(CC) -c $(CFLAGS) $(INCLUDES) terminal.c

rowscreen.o: rowscreen.c rowscreen.h tvi.h
	$(CC) -c $(CFLAGS) $(INCLUDES) rowscreen.c

clean:
	$(RM) *.o $(MAIN)
