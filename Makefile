# Makefile for memwatch
# use this file to build for Mac OS X or Linux systems

CC=gcc
OBJECTS=memwatch.o vmmap.o memory_search.o search_data.o vmmap_data.o process_utils.o parse_utils.o freeze_region.o search_data_list.o
CFLAGS=-g -Wall
LDFLAGS=-lreadline
EXECUTABLES=memwatch value_test value_test_float register_test32 register_test64

all: $(EXECUTABLES)

install: memwatch
	cp memwatch /usr/bin/memwatch

memwatch: $(OBJECTS)
	g++ $(LDFLAGS) -o memwatch $^

value_test: value_test.c
	g++ $(LDFLAGS) -o value_test $^

value_test_float: value_test_float.c
	g++ $(LDFLAGS) -o value_test_float $^

register_test64: register_test64.s
	g++ $(LDFLAGS) -m64 -o register_test64 $^
register_test32: register_test32.s
	g++ $(LDFLAGS) -m32 -o register_test32 $^

clean:
	-rm *.o $(EXECUTABLES)

.PHONY: clean
