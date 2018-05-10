CFLAGS=-g -Wall -Werror
CXXFLAGS=-g -Wall -Werror -I/usr/local/include -I/opt/local/include
EXECUTABLES=array_test value_test value_test_float

all: $(EXECUTABLES)

array_test: array_test.cc
	$(CXX) $(CXXFLAGS) -o array_test $^
value_test: value_test.cc
	$(CXX) $(CXXFLAGS) -o value_test $^
value_test_float: value_test_float.cc
	$(CXX) $(CXXFLAGS) -o value_test_float $^

clean:
	rm -rf *.o *.dSYM $(EXECUTABLES)

.PHONY: clean
