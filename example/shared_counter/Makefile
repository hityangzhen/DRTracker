CC := gcc
CFLAGS := -Werror -g -fno-omit-frame-pointer -pthread

targets := $(basename $(wildcard *.c))

all:$(targets)

clean:
	rm -rf *.o