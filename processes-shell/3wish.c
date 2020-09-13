
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

#include <assert.h>
#include <readline/readline.h>
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

enum ACTION { EXIT, NONE, PIPE, ASYNC, REDIRECT };

void signal_error(int throw, int hard) {
  if (throw) {
    write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
    if (hard) {
      exit(1);
    }
  }
}

// A vector of pointers
struct PVec {
  size_t size;  // holds the number of elements held
  size_t alloc; // the number of slots allocated
  void **start; // the start of the allocation
};
typedef struct PVec PVec;

// Creates a `PVec`.
PVec pvec_make() {
  PVec out;
  out.size = 0;
  out.alloc = 0;
  out.start = NULL;
  return out;
}

// Pushes en element `el` onto a PVec `vec`
void pvec_push(PVec *vec, void *el) {
  // we need to allocate more memory
  if (vec->alloc <= vec->size) {
    if (vec->start == NULL) {
      vec->alloc = 1;
      vec->start = malloc(sizeof(void *) * vec->alloc);
    } else {
      vec->alloc = vec->alloc * 2;
      vec->start = realloc(vec->start, vec->alloc * sizeof(void *));
    }
  }
  vec->start[vec->size++] = el;
}

// Pops an element of of `vec`.
// Returns NULL if no element is found.
void *pvec_pop(PVec *vec) {
  if (vec->size == 0) {
    return NULL;
  } else {
    return vec->start[--vec->size];
  }
}

// Frees each element in `vec`, then frees the vector itself.
void pvec_free(PVec *vec) {
  while (vec->size > 0)
    free(pvec_pop(vec));
  free(vec->start);
}

// Prints each element of `vec` with `line` used as the descriptor for `printf`.
void pvec_print(PVec *vec, char *line) {
  for (size_t i = 0; i < vec->size; i++) {
    printf(line, vec->start[i]);
  }
}

// Test if `c` represents the end of a line
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

// Tests for special operators. A special operator works like a single charicter
// word with respect to parsing.
int special_char_p(char c) { return c == '>'; }

// test if c represents an argument seperator
int arg_seperator_p(char c) { return c == ' ' || c == '\t'; }

// Parses a line into a list of arguments. `line` is the line to be parsed, it
// is not consumed. `index` is the location in line where parsing begins. It
// will be updated the point to the last char parsed. `action` is the status of
// each line. It is here to detect EOF.
PVec seperate_line(char *line, int *index, enum ACTION *action) {
  if (line == NULL) {
    return pvec_make();
  }
  PVec vec = pvec_make();
  int word_start;
  while (!end_of_line_p(line[(*index)], action)) {
    if (special_char_p(line[*index])) {
      (*index)++;
      word_start = *index - 1;
    } else {
      word_start = *index;
      while (!arg_seperator_p(line[*index]) &&
             !end_of_line_p(line[*index], action) &&
             !special_char_p(line[*index])) {
        (*index)++;
      }
    }

    int word_length = *index - word_start;
    if (word_length > 0) {
      char *str = (char *)malloc(sizeof(char) * (word_length + 1));
      signal_error(str == NULL, 0);
      memcpy(str, line + word_start, word_length);
      str[word_length] = '\0';
      pvec_push(&vec, str);
    }
    if (arg_seperator_p(line[*index]))
      (*index)++;
  }
  (*index)++;
  return vec;
}

// Resolves `command` to it's absolute path by iterating through `path`,
// returing the first entry found. If no valid entry is found, returns `NULL`.
char *resolve_path(char *command, PVec *path) {
  int cmd_len = strlen(command);
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
    strcat(hold_str, command);
    if (!access(hold_str, X_OK))
      return hold_str;
  }
  free(hold_str);
  return NULL;
}

// Takes a command `cmd`, a pointer to a string `file`.
//
// Return:
// < 0 => an error occured.
// = 0 => no redirect detected.
// > 0 => redirect detected cmd and file modified.
int handle_redirect(PVec *cmd, char **file) {
  size_t index = 0;
  for (; index < cmd->size; index++) {
    if (!strcmp(cmd->start[index], ">")) {
      if (index + 2 == cmd->size) {
        *file = (char *)pvec_pop(cmd);
        free(pvec_pop(cmd));
        return 1;
      } else {
        return -1;
      }
    }
  }
  return 0;
}

// Exectues an non-builtin command. `cmd` is the parsed command to be executed.
// `path`, `action`, and `processes` are treated the same as `exec_command`.
void exec_external(PVec *cmd, PVec *path, enum ACTION *signal,
                   PVec *processes) {
  char *absolute_path = resolve_path(cmd->start[0], path);
  free(cmd->start[0]);
  cmd->start[0] = absolute_path;
  if (absolute_path != NULL) {
    pid_t pd = fork();
    if (pd == 0) {
      char *pipe_to;
      int handle_res = handle_redirect(cmd, &pipe_to);
      if (handle_res > 0) {
        fclose(stdout);
        fopen(pipe_to, "w");
      } else if (handle_res < 0) {
        signal_error(1, 0);
        return;
      } else if (*signal == PIPE) {
      }
      pvec_push(cmd, NULL);
      execv(absolute_path, (char **)cmd->start);
    } else if (pd > 0) {
      int status;
      pvec_push(processes, (void *)(long)pd);
      waitpid(pd, &status, *signal == ASYNC);
      // TODO: report status when variables are implemented
    } else {
      // pd < 0
      signal_error(1, 0);
    }
  } else {
    signal_error(1, 0);
  }
}

// `cmd` is a the parsed command instruction. Calling exec_command gives
// ownership of `cmd` to the callee. `path` is the exec-path for the shell. It
// can be mutated but will end valid. `action` represents the current action
// state, and can be mutated. `processes` is a list of processes left running,
// and contains not pointers but integers. Processes can be added.
void exec_command(PVec *cmd, PVec *path, enum ACTION *action, PVec *processes) {
  if (!cmd->size || *action == EXIT) {
    return;
  }
  char *first = cmd->start[0];
  if (!strcmp(first, CD_COMMAND)) {
    signal_error(cmd->size != 2, 0);
    chdir(cmd->start[1]);
    pvec_free(cmd);
  } else if (!strcmp(first, EXIT_COMMAND)) {
    signal_error(cmd->size != 1, 0);
    *action = EXIT;
    pvec_free(cmd);
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
    exec_external(cmd, path, action, processes);
    pvec_free(cmd);
  }
}

// Executes the main logic of the shell:
// reading a line, then responding to it.
// input is the input source,
// batch = 1 => batch mode
// batch = 0 => not batch mode
void main_loop(FILE *input, int batch) {
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

  // running procces
  PVec processes = pvec_make();

  while (action != EXIT) {
    action = NONE;
    if (!batch) {
      line = readline(DEFAULT_PROMPT);
      if (line == NULL)
        nread = -1;
    } else {
      nread = getline(&line, &len, input);
    }
    if (nread < 0) {
      if (!batch)
        printf("\n");
      action = EXIT;
    }
    int line_end = strlen(line);
    if (line_end > 0)
      add_history(line);
    int read_to = 0;
    while (line_end > read_to && action != EXIT) {
      PVec command = seperate_line(line, &read_to, &action);
      exec_command(&command, &path, &action, &processes);
    }
    while (processes.size > 0) {
      int status;
      waitpid((int)(long)pvec_pop(&processes), &status, 0);
    }
    if (!batch) // for the readline library
      free(line);
  }
  if (batch) // for no readline library
    free(line);
  pvec_free(&path);
}

int main(int argc, char **argv) {
  if (argc == 1) {
    main_loop(stdin, 0);
  } else if (argc == 2) {
    FILE *file = fopen(argv[1], "r");
    if (file == NULL)
      signal_error(file == NULL, 1);
    else {
      main_loop(file, 1);
      fclose(file);
    }
  } else {
    if (0)
      printf("usage: wish [file]\n");
    else
      signal_error(1, 1);
  }
}
