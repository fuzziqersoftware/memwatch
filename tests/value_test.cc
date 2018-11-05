#include <sys/types.h>
#include <sys/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <phosg/Encoding.hh>


int main(int argc, char* argv[]) {

  printf("pid: %d\n", getpid());

  int8_t s8 = 0, rs8; // rs8 not really necessary but whatevs
  int16_t s16 = 0, rs16;
  int32_t s32 = 0, rs32;
  int64_t s64 = 0, rs64;

  for (;;) {
    printf("enter value: ");
    scanf("%lld", &s64);
    rs8 = s8 = s16 = s32 = s64;
    rs16 = bswap16(s16);
    rs32 = bswap32(s32);
    rs64 = bswap64(s64);

    printf("s8/s16/s32/s64     =  %02hhX / %04hX / %08X / %016llX\n",
           s8, s16, s32, s64);
    printf("s8/s16/s32/s64     =  %hhd / %hd / %d / %lld\n",
           s8, s16, s32, s64);
    printf("rs8/rs16/rs32/rs64 = %02hhX / %04hX / %08X / %016llX\n",
           rs8, rs16, rs32, rs64);
    printf("rs8/rs16/rs32/rs64 = %hhd / %hd / %d / %lld\n",
           rs8, rs16, rs32, rs64);
  }

  return 0;
}
