#include <stdio.h>
#include <stdlib.h>

int read_compressed_char(FILE *from, FILE *to) {
  int count;
  fread(&count, 4, 1, from);
  char c;
  int out = fread(&c, 1, 1, from);
  while (count-- > 0) {
    putc(c, to);
  }
  return out;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    printf("wunzip: file1 [file2 ...]\n");
    return 1;
  } else {
    for (int i = 1; i < argc; i++) {
      FILE *fp = fopen(argv[i], "r");
      if (fp == NULL)
        return 1;
      while (read_compressed_char(fp, stdout)) {
      }
      fclose(fp);
    }
  }
}
