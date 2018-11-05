#include <sys/types.h>
#include <sys/ptrace.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char* argv[]) {

  printf("pid: %d\n", getpid());

  int array_size = 20;
  uint64_t array[array_size];
  memset(array, 0, sizeof(uint64_t) * array_size);

  for (;;) {
    printf("%p:", array);

    int x;
    for (x = 0; x < array_size; x++) {
      printf(" %llu", array[x]);
    }
    printf("\n");

    sleep(1);
  }

  return 0;
}
