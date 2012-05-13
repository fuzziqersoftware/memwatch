# Makefile for memwatch tests

CC=gcc
CFLAGS=-g -Wall
EXECUTABLES=value_test value_test_float register_test32 register_test64 pt_deny_test

all: $(EXECUTABLES)

value_test: value_test.c
	g++ $(LDFLAGS) -o value_test $^
value_test_float: value_test_float.c
	g++ $(LDFLAGS) -o value_test_float $^
register_test64: register_test64.s
	g++ $(LDFLAGS) -m64 -o register_test64 $^
register_test32: register_test32.s
	g++ $(LDFLAGS) -m32 -o register_test32 $^
pt_deny_test: pt_deny_test.c
	g++ $(LDFLAGS) -o pt_deny_test $^

clean:
	rm -rf *.o *.dSYM $(EXECUTABLES)

.PHONY: clean
