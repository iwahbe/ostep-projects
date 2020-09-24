#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHUNK_SIZE 1000
#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

void write_compressed_char(FILE *to, char c, int *count) {
  if (*count > 0) {
    fwrite(count, 4, 1, to);
    putc(c, to);
    *count = 0;
  }
}

typedef struct {
  char last_char;
  int count;
} CompressInfo;

void write_compressed_file(FILE *old_file, FILE *to, CompressInfo *info) {
  char c;
  while ((c = getc(old_file)) != EOF) {
    if (c == info->last_char) {
      (info->count)++;
    } else {
      write_compressed_char(to, info->last_char, &(info->count));
      info->last_char = c;
      info->count = 1;
    }
  }
}

int main(int argc, char **argv) {
  if (argc == 1) {
    printf("pzip: file1 [file2 ...]\n");
    return 1;
  } else {
    CompressInfo info;
    info.last_char = EOF;
    info.count = 0;
    for (int i = 1; i < argc; i++) {
      int fp = open(argv[i], "r");
      if (fp == NULL)
        return 1;
      write_compressed_file(fp, stdout, &info);
      fclose(fp);
    }
    write_compressed_char(stdout, info.last_char, &(info.count));
  }
}

typedef struct {
  int tnum;    // task number
  char *start; // start of task
  char *end;   // end of task
} Task;

void dispatch(char *start, char *end, int tnum) {}

void thread_write_compressed_file(int input, CompressInfo *info,
                                  Task **thread_pool) {
  struct stat sb;
  if (fstat(input, &sb) == -1) {
    handle_error("fstat");
  }
  char *addr, *start, *end;
  start = end = addr = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, input, 0);
  if (addr == NULL)
    handle_error("mmap");
  int tnum = 0;

  while (start - addr + CHUNK_SIZE > sb.st_size) {
    end = start + CHUNK_SIZE;
    while (end - addr + 1 < sb.st_size && *end == *(end + 1))
      end++; // ensure that we dont break in the middle of the same char
    dispatch(start, end, tnum++); // dispatch a new task to the que
    start = end + 1;
  }
  dispatch(start, addr + sb.st_size, tnum++);
}
