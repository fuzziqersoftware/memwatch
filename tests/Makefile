CFLAGS=-g -Wall -Werror
CXXFLAGS=-g -Wall -Werror -I/usr/local/include
EXECUTABLES=array_test value_test value_test_float register_test32 register_test64

all: $(EXECUTABLES)

array_test: array_test.cc
	$(CXX) $(CXXFLAGS) -o array_test $^
value_test: value_test.cc
	$(CXX) $(CXXFLAGS) -o value_test $^
value_test_float: value_test_float.cc
	$(CXX) $(CXXFLAGS) -o value_test_float $^
register_test64: register_test64.s
	gcc $(LDFLAGS) -m64 -o register_test64 $^
register_test32: register_test32.s
	gcc $(LDFLAGS) -m32 -o register_test32 $^

clean:
	rm -rf *.o *.dSYM $(EXECUTABLES)

.PHONY: clean
