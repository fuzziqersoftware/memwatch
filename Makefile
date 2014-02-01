CC=gcc
OBJECTS=memwatch.o vmmap.o memory_search.o search_data.o vmmap_data.o process_utils.o parse_utils.o freeze_region.o search_data_list.o
CFLAGS=-O3 -s -Wall -D__MACOSX
LDFLAGS=-lreadline
EXECUTABLES=memwatch

all: memwatch tests

memwatch: $(OBJECTS)
	g++ $(LDFLAGS) -o memwatch $^

install: memwatch
	cp memwatch /usr/bin/memwatch
	cp memwatch.1 /usr/local/share/man/man1/memwatch.1

tests:
	cd tests/ && make && cd ..

clean:
	-rm -f *.o $(EXECUTABLES)
	cd tests/ && make clean && cd ..

.PHONY: clean tests
