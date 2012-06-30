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

  int8_t s8 = 0, ls8; // ls8 not really necessary but whatevs
  int16_t s16 = 0, ls16;
  int32_t s32 = 0, ls32;
  int64_t s64 = 0, ls64;

  for (;;) {
    printf("enter value: ");
    scanf("%lld", &s64);
    ls8 = s8 = s16 = s32 = s64;

    // omg ugly byteswapping code
    ls16 = ((s16 & 0xFF) << 8) | ((s16 >> 8) & 0xFF);
    ls32 = ((s32 & 0xFF) << 24) | (((s32 >> 8) & 0xFF) << 16)
         | (((s32 >> 16) & 0xFF) << 8) | ((s32 >> 24) & 0xFF);
    ls64 = ((s64 & 0xFF) << 56) | (((s64 >> 8) & 0xFF) << 48)
         | (((s64 >> 16) & 0xFF) << 40) | (((s64 >> 24) & 0xFF) << 32)
         | (((s64 >> 32) & 0xFF) << 24) | (((s64 >> 40) & 0xFF) << 16)
         | (((s64 >> 48) & 0xFF) << 8) | ((s64 >> 56) & 0xFF);

    printf("s8/s16/s32/s64: %02hhX / %04hX / %08X / %016llX\n",
           s8, s16, s32, s64);
    printf("s8/s16/s32/s64: %hhd / %hd / %d / %lld\n", s8, s16, s32, s64);
    printf("l8/l16/l32/l64: %02hhX / %04hX / %08X / %016llX\n",
           ls8, ls16, ls32, ls64);
    printf("l8/l16/l32/l64: %hhd / %hd / %d / %lld\n", ls8, ls16, ls32, ls64);
  }

  return 0;
}
