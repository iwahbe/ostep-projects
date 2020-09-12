
/*
 * support batch mode (where given an input file)
 * Use getline()
 *
 * consider strsep() to parse
 *
 * execv() for running commands
 *
 *
 *
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_size_t.h>

const char *PROMPT_VAR = "PS1";
const char *DEFAULT_PROMPT = "wish> ";

const char *PATH_COMMAND = "path";
const char *DEFAULT_PATH = "/bin";

const char *CD_COMMAND = "cd";
const char *EXIT_COMMAND = "exit";

const char *ERROR_MESSAGE = "An error has occurred\n";

void signal_error(int throw) {
  if (throw) {
    fwrite(ERROR_MESSAGE, sizeof(char), sizeof(ERROR_MESSAGE), stderr);
    exit(1);
  }
}

// A vector of pointers
struct PVec {
  size_t size;  // holds the number of elements held
  size_t alloc; // the number of slots allocated
  void **start; // the start of the allocation
};
typedef struct PVec PVec;

PVec pvec_make() {
  PVec out;
  out.size = 0;
  out.alloc = 0;
  out.start = NULL;
  return out;
}

void pvec_push(PVec *vec, void *el) {
  // we need to allocate more memory
  if (vec->alloc <= vec->size) {
    if (vec->start == NULL)
      vec->start = malloc(sizeof(void *) * 2);
    else
      vec->start = realloc(vec->start,
                           sizeof(void *) * (vec->alloc ? vec->alloc * 2 : 2));
  }
  vec->start[vec->size++] = el;
}

void *pvec_pop(PVec *vec) {
  if (vec->size == 0) {
    return NULL;
  } else {
    return vec->start[--vec->size];
  }
}

void pvec_free(PVec *vec) {
  while (vec->size > 0)
    free(pvec_pop(vec));
  free(vec->start);
}

// test if c represents the end of a line
int end_of_line_p(char c) { return c == '\n' || c == ';' || c == '\0'; }

// test if c represents an argument seperator
int arg_seperator_p(char c) { return c == ' ' || c == '\t'; }

// Returns a PVec of the arguments
PVec seperate_line(char *line, int *end) {
  *end = 0;
  if (line == NULL) {
    return pvec_make();
  }
  PVec vec = pvec_make();
  while (!end_of_line_p(line[(*end)])) {
    int word_start = *end;
    while (!arg_seperator_p(line[*end]) && !end_of_line_p(line[*end])) {
      (*end)++;
    }
    int word_length = *end - word_start;
    if (word_length > 0) {
      char *str = (char *)malloc(sizeof(char) * (word_length + 1));
      signal_error(str == NULL);
      memcpy(str, line + word_start, word_length);
      str[word_length] = '\0';
      pvec_push(&vec, str);
    }
    if (arg_seperator_p(line[*end]))
      (*end)++;
  }
  return vec;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Bad arguments");
    exit(1);
  }
  for (int i = 1; i < argc; i++) {
    int end_of_read = 0;
    PVec line = seperate_line(argv[i], &end_of_read);
    printf("Line read in: '%s' for %d chars and %zu arguments.\n", argv[i],
           end_of_read, line.size);
    for (size_t i = 0; i < line.size; i++) {
      printf("Arg read: '%s'\n", line.start[i]);
    }
    pvec_free(&line);
  }
}
