#Windows check
ifeq ($(OS), Windows_NT)
    CC = x86_64-w64-mingw32-gcc
    CFLAGS = -shared -fPIC -Wall
    LDFLAGS = -shared
    OUT_EXT = .wcx
    LIBEXT = .wcx
    OUTPUT_NAME = yay0.wcx
else  # Use GCC
    CC = gcc
    CFLAGS = -shared -fPIC -Wall
    LDFLAGS = -shared
    OUT_EXT = .wcx
    LIBEXT = .wcx
    OUTPUT_NAME = yay0.wcx
endif

SRCS = wcx.c yay0.c yay0encoder.c
OBJS = $(SRCS:.c=.o)
LIBRARY = yay0.wcx

$(LIBRARY): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(LIBRARY)
