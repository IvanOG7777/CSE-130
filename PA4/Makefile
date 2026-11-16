############################## Configuration ##############################
#
# This Makefile compiles all of the .c files that are found in the current
# directory into their corresponding .o files.  Then it links the .o files
# and the .a file LIBRARY into EXEBIN.
#
# The Makefile assumes that all of the .h files that are found in the
# current directory are part of the project.  To ensure that all .o files
# are up-to-date, the Makefile assumes that every .o file depends on all of
# the .h files.
#
# Dr. Veenstra
#

EXECBIN  = httpserver
LIBRARY  = asgn4_helper_funcs.a


############################### Rest of Makefile ##############################

CC       = clang
FORMAT   = clang-format
CFLAGS   = -Wall -Wpedantic -Werror -Wextra -DDEBUG

# Assume that all .c files in the current directory are part of the project.
SOURCES  = $(wildcard *.c)
OBJECTS  = $(SOURCES:%.c=%.o)

# Used to ensure that everything is recompiled whenever a .h file changes.
HEADERS  = $(wildcard *.h)

.PHONY: all
all: $(EXECBIN)

$(EXECBIN): $(OBJECTS) $(LIBRARY)
	$(CC) $^ -o $@

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(EXECBIN) $(OBJECTS)

.PHONY: format
format:
	$(FORMAT) -i $(SOURCES) $(HEADERS)
