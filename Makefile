# makefile for tvi

CC = gcc
DFLAGS = -g
CFLAGS = -Wall -Wextra -pedantic -std=c99
# LFLAGS = -L...
LFLAGS = 
# LIBS = -l... -lm
LIBS =
INCLUDES =

SRCS = tvi.c highlight.c terminal.c rowscreen.c
HDRS = $(SRCS:.c=.h)

OBJS = $(SRCS:.c=.o)
OBJSRLS = $(addprefix release/, $(OBJS))
OBJSDBG = $(addprefix debug/, $(OBJS))

MAIN = tvi

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) -o release/$(MAIN) $(LFLAGS) $(LIBS) $(OBJSRLS)
	$(CC) -o debug/$(MAIN) $(LFLAGS) $(LIBS) $(OBJSDBG)

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o release/$@
	$(CC) -c $(DFLAGS) $(CFLAGS) $(INCLUDES) $< -o debug/$@

clean:
	$(RM) release\$(MAIN)
	$(RM) release\*.o
	$(RM) debug\$(MAIN)
	$(RM) debug\*.o
