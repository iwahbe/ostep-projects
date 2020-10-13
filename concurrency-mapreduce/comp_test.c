#include "mapreduce.h"
#include <stdio.h>

void reduce(char *key, Getter get_func, int partition_number) {
  printf("reduce called with key: '%s', partition_numer: '%d'\n", key,
         partition_number);
  get_func("get_func", 9);
}

unsigned long partition(char *key, int num_partition) {
  printf("partition called on key: '%s', num_partition: '%d'\n", key,
         num_partition);
  return 0;
}

void map(char *file_name) {
  printf("map called with file_name: '%s'\n", file_name);
}

int main(int argc, char **argv) {
  int num_mappers = 2;
  int num_reducers = 5;
  MR_Run(argc, argv, map, num_mappers, reduce, num_reducers, partition);
}
