CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic

httpserver: httpserver.o asgn2_helper_funcs.a
	$(CC) $(CFLAGS) -o httpserver httpserver.o asgn2_helper_funcs.a

httpserver.o: httpserver.c
	$(CC) $(CFLAGS) -c httpserver.c

all: httpserver

clean:
	rm -f httpserver httpserver.o