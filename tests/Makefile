# Makefile for memwatch tests

CC=gcc
CFLAGS=-g -Wall
EXECUTABLES=array_test value_test value_test_float register_test32 register_test64

all: $(EXECUTABLES)

array_test: array_test.c
	gcc $(LDFLAGS) -o array_test $^
value_test: value_test.c
	gcc $(LDFLAGS) -o value_test $^
value_test_float: value_test_float.c
	gcc $(LDFLAGS) -o value_test_float $^
register_test64: register_test64.s
	gcc $(LDFLAGS) -m64 -o register_test64 $^
register_test32: register_test32.s
	gcc $(LDFLAGS) -m32 -o register_test32 $^

clean:
	rm -rf *.o *.dSYM $(EXECUTABLES)

.PHONY: clean
