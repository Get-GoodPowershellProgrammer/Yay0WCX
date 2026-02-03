CC = gcc
CFLAGS = -fPIC -Wall
LDFLAGS = -shared

SRCS = wcx.c yay0.c
OBJS = $(SRCS:.c=.o)
LIBRARY = yay0.wcx

$(LIBRARY): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(LIBRARY)
