#include <stdio.h>
#include <stdlib.h>
#include <sys/_types/_size_t.h>
#include <sys/_types/_ssize_t.h>

void print_help() {
  printf("wget -- use\n");
  printf("Takes at least one file name, and prints the file's contents to "
         "stdout.\n");
  printf("Use:\n\twget filename+\n");
}

int print_file(char *filename) {
  FILE *file = fopen(filename, "r");
  if (file == NULL)
    return 1;
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, file)) > 0) {
    fwrite(line, linelen, 1, stdout);
  }
  if (line != NULL)
    free(line);
  return fclose(file);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    return 0;
    // print_help();
    return 1;
  } else {
    for (int i = 1; i < argc; i++) {
      if (print_file(argv[i])) {
        printf("wcat: cannot open file\n");
        return 1;
      }
    }
  }
}
