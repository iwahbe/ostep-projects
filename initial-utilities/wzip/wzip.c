#include <stdio.h>
#include <stdlib.h>

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
    printf("wzip: file1 [file2 ...]\n");
    return 1;
  } else {
    CompressInfo info;
    info.last_char = EOF;
    info.count = 0;
    for (int i = 1; i < argc; i++) {
      FILE *fp = fopen(argv[i], "r");
      if (fp == NULL)
        return 1;
      write_compressed_file(fp, stdout, &info);
      fclose(fp);
    }
    write_compressed_char(stdout, info.last_char, &(info.count));
  }
}
