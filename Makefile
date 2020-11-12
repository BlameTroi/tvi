# makefile for tvi

CC = gcc
CFLAGS = -g -Wall -Wextra -pedantic -std=c99
# LFLAGS = -L...
LFLAGS = -g
# LIBS = -l... -lm
LIBS =
INCLUDES =

SRCS = tvi.c highlight.c terminal.c rowscreen.c
# OBJS = $(SRCS:.c=.o)
OBJS = tvi.o highlight.o
HDRS = $(SRCS:.c=.h)

# $(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

MAIN = tvi

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) -o $(MAIN) $(LFLAGS) $(LIBS) $(OBJS)

tvi.o: $(SRCS) $(HDRS)
	$(CC) -c $(CFLAGS) $(INCLUDES) tvi.c terminal.c rowscreen.c

highlight.o: highlight.c highlight.h tvi.h
	$(CC) -c $(CFLAGS) $(INCLUDES) highlight.c

#terminal.o: terminal.c terminal.h
#	$(CC) -c $(CFLAGS) $(INCLUDES) terminal.c

clean:
	$(RM) *.o $(MAIN)
