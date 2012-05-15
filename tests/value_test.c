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

  int8_t s8 = 0;
  int16_t s16 = 0;
  int32_t s32 = 0;
  int64_t s64 = 0;

  for (;;) {
    printf("enter value: ");
    scanf("%lld", &s64);
    s8 = s16 = s32 = s64;

    printf("8/16/32/64: %hhd / %hd / %d / %lld\n", s8, s16, s32, s64);
  }

  return 0;
}
