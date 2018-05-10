OBJECTS=Main.o MemwatchShell.o MemorySearch.o ProcessMemoryAdapter.o RegionFreezer.o Signalable.o
CXXFLAGS=-O3 -g -Wall -Werror -std=c++14 -I/opt/local/include -I/usr/local/include
LDFLAGS=-lreadline -L/opt/local/lib -L/usr/local/lib -lphosg
EXECUTABLES=memwatch

all: memwatch tests

memwatch: $(OBJECTS)
	g++ $(LDFLAGS) -o memwatch $^

install: memwatch
	cp memwatch /usr/local/bin/memwatch
	cp memwatch.1 /usr/local/share/man/man1/memwatch.1

tests:
	cd tests/ && make && cd ..

clean:
	-rm -f *.o $(EXECUTABLES)
	cd tests/ && make clean && cd ..

.PHONY: clean tests
