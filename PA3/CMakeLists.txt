cmake_minimum_required(VERSION 3.28)

project(PA3 C)

set(CMAKE_C_STANDARD 17)

find_package(Threads REQUIRED)

add_executable(basicPushPop
        queue_users/basic_push_pop.c
        queue.c
        queue.h
)

add_executable(basicLockUnlock
        rwlock_users/basic_lock_unlock.c
        rwlock.c
        rwlock.h
)