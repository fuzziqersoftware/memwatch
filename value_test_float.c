#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {

  float f = 0;
  double d = 0;

  for (;;) {
    printf("enter value: ");
    scanf("%lf", &d);
    f = d;

    printf("d = %lf\n", d);
    printf("f = %f\n", f);
  }

  return 0;
}
