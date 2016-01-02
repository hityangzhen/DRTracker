LINUX_ENV=$(shell getconf LONG_BIT)

ifeq ($(LINUX_ENV), 32)
MACHINE=i686
else
MACHINE=x86-64
endif

all: lf_queue.c lf_queue.h unittest.h thread_wrappers_pthread.h
	gcc -DATOMIC -g -o  unittest unittest.c lf_queue.c -march=${MACHINE} -pthread
