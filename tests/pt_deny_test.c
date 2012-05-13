// value_test.c, by Martin Michelsen, 2012
// simple app that repeatedly asks for a new value for its internal variables
// useful for testing memory search routines

#include <sys/types.h>
#include <sys/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {

  ptrace(PT_DENY_ATTACH, getpid(), NULL, 0);
  printf("I bet you can\'t ptrace me! haha!\n");

  for (;;)
    usleep(1000000);

  return 0;
}
