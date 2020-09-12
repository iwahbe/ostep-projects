
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
#include <sys/_types/_pid_t.h>
#include <sys/_types/_size_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <unistd.h>

const char *PROMPT_VAR = "PS1";
const char *DEFAULT_PROMPT = "wish> ";

const char *PATH_COMMAND = "path";
const char *DEFAULT_PATH = "/bin";

const char *CD_COMMAND = "cd";
const char *EXIT_COMMAND = "exit";

const char ERROR_MESSAGE[30] = "An error has occurred\n";

enum ACTION { EXIT, NONE, PIPE, ASYNC };

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
    if (vec->start == NULL) {
      vec->alloc = 1;
      vec->start = malloc(sizeof(void *) * vec->alloc);
    } else {
      vec->alloc = vec->alloc * 2;
      vec->start = realloc(vec->start, vec->alloc * sizeof(void *));
      signal_error(vec->start == NULL);
    }
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
int end_of_line_p(char c, enum ACTION *type) {
  if (c == '\n' || c == ';' || c == '\0') {
    *type = NONE;
    return 1;
  } else if (c == EOF) {
    *type = EXIT;
    return 1;
  } else if (c == '|') {
    *type = PIPE;
    return 1;
  } else if (c == '&') {
    *type = ASYNC;
    return 1;
  } else {
    return 0;
  }
}

// test if c represents an argument seperator
int arg_seperator_p(char c) { return c == ' ' || c == '\t'; }

// Returns a PVec of the arguments
PVec seperate_line(char *line, int *end, enum ACTION *signal) {
  if (line == NULL) {
    return pvec_make();
  }
  PVec vec = pvec_make();
  while (!end_of_line_p(line[(*end)], signal)) {
    int word_start = *end;
    while (!arg_seperator_p(line[*end]) && !end_of_line_p(line[*end], signal)) {
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
  (*end)++;
  return vec;
}

char *resolve_path(char *cmd, PVec *path) {
  int cmd_len = strlen(cmd);
  int hold_str_len = cmd_len;
  char *hold_str = malloc(sizeof(char) * hold_str_len);
  for (size_t i = 0; i < path->size; i++) {
    int new_size = strlen(path->start[i]) + cmd_len;
    if (new_size > hold_str_len) {
      hold_str = realloc(hold_str, sizeof(char) * (new_size + 2));
      hold_str_len = new_size;
    }
    strcpy(hold_str, path->start[i]);
    strcat(hold_str, "/");
    strcat(hold_str, cmd);
    if (!access(hold_str, X_OK))
      return hold_str;
  }
  free(hold_str);
  return NULL;
}

void exec_external(PVec *cmd, PVec *path) {
  char *absolute_path = resolve_path(cmd->start[0], path);
  if (absolute_path != NULL) {
    pid_t pd = fork();
    if (pd == 0) {
      pvec_push(cmd, NULL);
      execv(absolute_path, (char **)cmd->start + 1);
    } else if (pd > 0) {
      wait(&pd);
    } else {
      // pd < 0
      signal_error(1);
    }
    free(absolute_path);
  } else {
    printf("Could not find command: %s\n", cmd->start[0]);
  }
  return;
}

void exec_command(PVec *cmd, PVec *path, enum ACTION *action) {
  *action = NONE;
  if (!cmd->size || *action == EXIT) {
    return;
  }
  char *first = cmd->start[0];
  if (!strcmp(first, CD_COMMAND)) {
    signal_error(cmd->size != 2);
    chdir(cmd->start[1]);
    free(cmd->start[1]);
  } else if (!strcmp(first, EXIT_COMMAND)) {
    signal_error(cmd->size != 1);
    *action = EXIT;
  } else if (!strcmp(first, PATH_COMMAND)) {
    size_t index = 1;
    // if the first command is "-+", then don't remove the old path
    if (cmd->size > 1 && !(strcmp(cmd->start[1], "-+"))) {
      index = 2;
    } else {
      while (pvec_pop(path) != NULL) {
      }
    }
    for (; index < cmd->size; index++) {
      pvec_push(path, cmd->start[index]);
    }
  } else {
    exec_external(cmd, path);
  }
  free(first);
}

void main_loop() {
  // setup path
  PVec path = pvec_make();
  char *defpath = malloc(sizeof(char) * (strlen(DEFAULT_PATH) + 1));
  strcpy(defpath, DEFAULT_PATH);
  pvec_push(&path, defpath);

  // setup getline
  char *line = NULL;
  size_t len = 0;
  ssize_t nread;
  enum ACTION action = NONE;
  while (action != EXIT) {
    printf("%s", DEFAULT_PROMPT);
    nread = getline(&line, &len, stdin);
    if (nread < 0) {
      printf("\n");
      return;
    }
    int line_end = strlen(line);
    int read_to = 0;
    while (line_end != read_to && action != EXIT) {
      PVec command = seperate_line(line, &read_to, &action);
      exec_command(&command, &path, &action);
    }
  }
}

int main(int argc, char **argv) {
  if (argc == 1) {
    main_loop();
  } else if (argc == 2) {
    printf("TODO: read in batch file");
    exit(2);
  } else {
    printf("Bad arguments");
    exit(1);
  }
}
