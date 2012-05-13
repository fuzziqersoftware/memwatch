# Makefile for memwatch
# use this file to build for Mac OS X or Linux systems

include Makefile.inc

memwatch: $(OBJECTS)
	g++ $(LDFLAGS) -o memwatch $^
