# Makefile for memwatch
# use this file to build for Mac OS X or Linux systems

CC=gcc
OBJECTS=memwatch.o vmmap.o memory_search.o search_data.o vmmap_data.o process_utils.o parse_utils.o freeze_region.o search_data_list.o
CFLAGS=-g -Wall
LDFLAGS=-lreadline
EXECUTABLES=memwatch

all: $(EXECUTABLES)
	cd tests/ && make && cd ..

install: memwatch
	cp memwatch /usr/bin/memwatch
	cp memwatch.1 /usr/local/share/man/man1/memwatch.1

clean:
	-rm *.o $(EXECUTABLES)
	cd tests/ && make clean && cd ..

.PHONY: clean
