CC = clang

CFLAGS = -Wall -Wextra -Werror -pedantic

TARGETS = basic_push_pop basic_lock_unlock

all: $(TARGETS)

queue_users/basic_push_pop.o: queue_users/basic_push_pop.c queue.h
	$(CC) $(CFLAGS) -c queue_users/basic_push_pop.c -o $@

rwlock_users/basic_lock_unlock.o: rwlock_users/basic_lock_unlock.c rwlock.h
	$(CC) $(CFLAGS) -c rwlock_users/basic_lock_unlock.c -o $@

basic_push_pop: queue_users/basic_push_pop.o queue.o
	$(CC) $(LDFLAGS) -o $@ $^

basic_lock_unlock: rwlock_users/basic_lock_unlock.o rwlock.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o queue_users/*.o rwlock_users/*.o $(TARGETS)

.PHONY: all clean