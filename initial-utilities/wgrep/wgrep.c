#include <stdio.h>
#include <stdlib.h>
#include <sys/_types/_size_t.h>
#include <sys/_types/_ssize_t.h>

int match(char *search, char *string);
int search_file(char *filename, char *search_term);

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("wgrep: searchterm [file ...]\n");
    return 1;
  } else if (argc == 2) {
    search_file(NULL, argv[1]);
  } else {
    for (int i = 2; i < argc; i++)
      if (search_file(argv[i], argv[1])) {
        return 1;
      }
  }
}

int match(char *search, char *string) {
  for (int loc = 0; string[loc] != '\0'; loc++) {
    // printf("examined: %c\n", string[loc]);
    int ploc = loc;
    for (int sloc = 0;; sloc++) {
      if (search[sloc] == '\0') {
        return loc;
      } else if (search[sloc] != string[ploc++]) {
        break;
      }
    }
  }
  return -1;
}

int search_file(char *filename, char *search_term) {
  FILE *fp = NULL;
  if (filename != NULL) {
    fp = fopen(filename, "r");
    if (fp == NULL) {
      printf("wgrep: cannot open file\n");
      return 1;
    }
  }
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen = 0;
  while ((linelen = getline(&line, &linecap, filename == NULL ? stdin : fp)) >
         0) {
    if (match(search_term, line) >= 0)
      fwrite(line, linelen, 1, stdout);
  }
  if (line != NULL)
    free(line);
  fclose(fp);
  return 0;
}
