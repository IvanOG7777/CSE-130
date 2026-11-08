CSE 130 -ASGN3 Concurrent Data Structures Assignment

Consists of two programs queue and rwlocks, both written in C.

queue.c, implements a circular buffer using head and tail values to track elements in the buffer/queue
It uses mutexes and condition variables to provide safe push and pops of elements between threads.

rwlock.c, implements a reader-writer lock that allows for multiple readers or one reader to access shared resources.
It supports three reader-writer priorities READER, WRITERS, and N_WAY.

Makefile, where we have the rules needed to compile the program with clang, also includes commands to.

README.md, which is this file explaing the program and its functionality

Compilation: To compile we use make or make all.

To clean we use make clean.