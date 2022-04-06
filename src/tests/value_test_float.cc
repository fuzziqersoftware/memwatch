#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <phosg/Encoding.hh>


int main(int, char**) {

  printf("pid: %d\n", getpid());

  float f = 0, rf;
  double d = 0, rd;

  int32_t* fp = (int32_t*)&f;
  int32_t* rfp = (int32_t*)&rf;
  int64_t* dp = (int64_t*)&d;
  int64_t* rdp = (int64_t*)&rd;

  for (;;) {
    printf("enter value: ");
    scanf("%lf", &d);
    f = d;

    *rfp = bswap32(*rfp);
    *rdp = bswap64(*rdp);

    printf("f/d   = %08X / %016llX\n", *fp, *dp);
    printf("f/d   = %g / %lg\n", f, d);
    printf("rf/rd = %08X / %016llX\n", *rfp, *rdp);
    printf("rf/rd = %g / %lg\n", rf, rd);
  }

  return 0;
}
