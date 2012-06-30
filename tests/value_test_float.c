// value_test_float.c, by Martin Michelsen, 2012
// simple app that repeatedly asks for a new value for its internal variables
// useful for testing memory search routines

#include <sys/types.h>
#include <sys/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {

  ptrace(PT_DENY_ATTACH, getpid(), NULL, 0);

  float f = 0, lf;
  double d = 0, ld;

  int32_t* fp = (int32_t*)&f;
  int32_t* lfp = (int32_t*)&lf;
  int64_t* dp = (int64_t*)&d;
  int64_t* ldp = (int64_t*)&ld;

  for (;;) {
    printf("enter value: ");
    scanf("%lf", &d);
    f = d;

    // omg ugly byteswapping code
    *lfp = ((*fp & 0xFF) << 24) | (((*fp >> 8) & 0xFF) << 16)
         | (((*fp >> 16) & 0xFF) << 8) | ((*fp >> 24) & 0xFF);
    *ldp = ((*dp & 0xFF) << 56) | (((*dp >> 8) & 0xFF) << 48)
         | (((*dp >> 16) & 0xFF) << 40) | (((*dp >> 24) & 0xFF) << 32)
         | (((*dp >> 32) & 0xFF) << 24) | (((*dp >> 40) & 0xFF) << 16)
         | (((*dp >> 48) & 0xFF) << 8) | ((*dp >> 56) & 0xFF);

    printf("f/d   = %08X / %016llX\n", *fp, *dp);
    printf("f/d   = %g / %lg\n", f, d);
    printf("lf/ld = %08X / %016llX\n", *lfp, *ldp);
    printf("lf/ld = %g / %lg\n", lf, ld);
  }

  return 0;
}
