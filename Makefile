CFLAGS=-g -O2 -Wall -Wextra -Isrc -rdynamic -DNDEBUG $(OPTFLAGS) -lrt
LDLIBS=-lm -lasound

all: cpiped

clean:
	rm -f cpiped
