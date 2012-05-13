# Makefile for memwatch
# use this file to build for Mac OS X or Linux systems

CC=gcc
OBJECTS=memwatch.o vmmap.o memory_search.o search_data.o vmmap_data.o process_utils.o parse_utils.o freeze_region.o search_data_list.o
CFLAGS=-g -Wall
LDFLAGS=-lreadline
EXECUTABLES=memwatch tests/value_test tests/value_test_float tests/register_test32 tests/register_test64

all: $(EXECUTABLES)

install: memwatch
	cp memwatch /usr/bin/memwatch

tests/value_test: tests/value_test.c
	g++ $(LDFLAGS) -o tests/value_test $^
tests/value_test_float: tests/value_test_float.c
	g++ $(LDFLAGS) -o tests/value_test_float $^
tests/register_test64: tests/register_test64.s
	g++ $(LDFLAGS) -m64 -o tests/register_test64 $^
tests/register_test32: tests/register_test32.s
	g++ $(LDFLAGS) -m32 -o tests/register_test32 $^

clean:
	-rm *.o $(EXECUTABLES)

.PHONY: clean
