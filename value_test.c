#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {

  //int8_t s8 = 0;
  //int16_t s16 = 0;
  int32_t s32 = 0;
  //int64_t s64 = 0;

  for (;;) {
    printf("enter value: ");
    scanf("%d", &s32);
    //s8 = s16 = s32 = s64;

    //printf("s8  = %hhd\n", s8);
    //printf("s16 = %hd\n", s16);
    printf("s32 = %d\n", s32);
    //printf("s64 = %lld\n", s64);
  }

  return 0;
}
