# Makefile for memwatch
# use this file to build for Mac OS X or Linux systems

EXTRA_CFLAGS=-D__MACOSX

include Makefile.inc

memwatch: $(OBJECTS)
	g++ $(LDFLAGS) -o memwatch $^
