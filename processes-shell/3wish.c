
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
#include <errno.h>
#include <stdio.h> // note stdio.h must come before readline
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

const char *PROMPT_VAR = "PS1";
const char *DEFAULT_PROMPT = "wish> ";

const char *PATH_COMMAND = "path";
const char *DEFAULT_PATH = "/bin";
const char *SHOW_PATH_COMMAND = "-e";
const char *ADD_PATH_COMMAND = "-+";

const char *CD_COMMAND = "cd";
const char *EXIT_COMMAND = "exit";

enum ACTION { EXIT = 0, NONE = 1, ASYNC = 2 };

const char TEST_ERROR_MESSAGE[30] = "An error has occurred\n";

#define ERROR_MESSAGE(s) ("3wish: " s)

void signal_error(int throw, int hard, char *message) {
  if (throw) {
#ifdef TEST_ERROR
    write(STDERR_FILENO, TEST_ERROR_MESSAGE, strlen(TEST_ERROR_MESSAGE));
#else
    perror(message ? message : ERROR_MESSAGE(""));
#endif
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

// Pops the nth element of `vec`.
// Returns NULL if no element is found.
void *pvec_pop_nth(PVec *vec, size_t n) {
  if (vec->size < n) {
    return NULL;
  } else {
    void *out = vec->start[n];
    size_t i = n + 1;
    while (i < vec->size) {
      vec->start[i - 1] = vec->start[i];
      i++;
    }
    vec->size--;
    return out;
  }
}

PVec pvec_split(PVec *vec, int(predicate)(void *), int append) {
  size_t index = 0;
  PVec out = pvec_make();
  while (index < vec->size && !predicate(vec->start[index])) {
    index++;
  }
  if (index == vec->size) {
    return out;
  }
  size_t old_size = vec->size;
  if (append > 0) {
    // the found element is included in the new vector
  } else if (append == 0) {
    if (vec->start[index] != NULL)
      free(vec->start[index]); // the found element is skipped
    index++;
    vec->size--;
  } else {
    index += 2; // the found element is left in the old vector;
  }
  for (; index < old_size; index++) {
    pvec_push(&out, vec->start[index]);
    vec->size--;
  }
  return out;
}

char *pvec_concat(PVec *vec, char *sep) {
  int sep_length = strlen(sep);
  int total_length = 0;
  for (size_t i = 0; i < vec->size; i++) {
    total_length += strlen(vec->start[i]) + sep_length;
  }
  // Concatonating an empty vector yields the empty string.
  if (total_length == 0) {
    char *empty = malloc(sizeof(char) * 1);
    *empty = '\0';
    return empty;
  }
  char *out = malloc(sizeof(char) * (total_length + 1));
  *out = '\0';
  for (size_t i = 0; i + 1 < vec->size; i++) {
    strcat(out, vec->start[i]);
    strcat(out, sep);
  }
  strcat(out, vec->start[vec->size - 1]);
  return out;
}

// Frees each element in `vec`, then frees the vector itself.
void pvec_free(PVec *vec) {
  while (vec->size > 0) {
    void *el = pvec_pop(vec);
    if (el != NULL)
      free(el);
  }
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
  } else if (c == '&') {
    *type = ASYNC;
    return 1;
  } else {
    return 0;
  }
}

// Tests for special operators. A special operator works like a single charicter
// word with respect to parsing.
int special_char_p(char c) { return c == '>' || c == '<' || c == '|'; }

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
      signal_error(str == NULL, 0, NULL);
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

int get_working_dir(char **buf, size_t *size) {
  // happily copied from the example for `getcwd`.
  long path_max;
  char *ptr;

  path_max = pathconf(".", _PC_PATH_MAX);
  if (path_max == -1)
    *size = 1024;
  else if (path_max > 10240)
    *size = 10240;
  else
    *size = path_max;
  ptr = NULL;
  for (*buf = malloc(sizeof(char) * *size); ptr == NULL; *size *= 2) {
    ptr = getcwd(*buf, *size);
    if (ptr == NULL && errno != ERANGE) {
      return 0;
    }
    if ((*buf = realloc(*buf, *size)) == NULL) {
      return 0;
    }
  }
  return 1;
}

PVec listify_path(char *path, int *error) {
  PVec list = pvec_make();
  *error = 0;
  char *cmd_cpy = malloc(sizeof(char) * (strlen(path) + 1));
  strcpy(cmd_cpy, path);
  char *pvar = NULL;
  int first = 0;
  while ((pvar = strsep(&cmd_cpy, "/")) != NULL) {
    if (!strcmp("", pvar) && first++ != 0)
      *error = 1;
    pvec_push(&list, pvar);
  }
  return list;
}

// Resolve `command` if `command` is an absolute path.
// Takes effect only if the leading char is '/' or '~'.
char *resolve_absolute(char *command) {
  if (command[0] == '~') {
    char *home = getenv("HOME");
    char *out = malloc(sizeof(char) * strlen(command) +
                       strlen(home)); // null terminator accounted for by the ~
    strcpy(out, home);
    strcat(out, command);
    return out;
  } else if (command[0] == '/' && !access(command, X_OK)) {
    char *out = malloc(sizeof(char) * (strlen(command) + 1));
    strcpy(out, command);
    return out;
  } else
    return NULL;
}

// TODO: merge relative and absolute

// Resolves relative to the current working directory.
char *resolve_relative(char *command) {
  PVec original;
  int error1 = 0;
  if (command[0] == '.') {
    // setup relative path
    size_t buf_size;
    char *origin_path;
    if (!get_working_dir(&origin_path, &buf_size)) {
      free(origin_path);
      return NULL;
    }
    original = listify_path(origin_path, &error1);
    free(origin_path);
  } else if (command[0] == '~') {
    // setup path from home dir
    char *home = getenv("HOME");
    original = listify_path(home, &error1);
  } else if (command[0] == '/' && !access(command, X_OK)) {
    // absolte path has no starting place
    original = pvec_make();
  } else {
    return NULL;
  }
  int error2;
  PVec delta = listify_path(command, &error2);
  if (error1 || error2) {
    pvec_free(&original);
    pvec_free(&delta);
    return NULL;
  }
  char *pvar;
  for (size_t i = 0; i < delta.size; i++) {
    pvar = delta.start[i];
    if (!strcmp(".", pvar)) {
    } else if (!strcmp("..", pvar)) {
      // truncate up
      free(pvec_pop(&original));
    } else {
      pvec_push(&original, pvar);
      delta.start[i] = malloc(1);
    }
  }
  char *out = pvec_concat(&original, "/");
  pvec_free(&original);
  pvec_free(&delta);
  if (!access(out, X_OK))
    return out;
  else {
    free(out);
    return NULL;
  }
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

// Resolves the path of `command`.
char *resolve_command(char *command, PVec *path) {
  if (command == NULL)
    return NULL;
  char *res = resolve_absolute(command);
  if (res == NULL)
    res = resolve_relative(command);
  if (res == NULL)
    res = resolve_path(command, path);
  return res;
}

// Takes a command `cmd`, a pointer to a string `file`.
//
// Return:
// < 0 => an error occured.
// = 0 => no redirect detected.
// > 0 => redirect detected cmd and file is the string of the input file.
int handle_redirect(PVec *cmd, char **file, char *trigger, char *after) {
  size_t index = 1; // We hold that the first char cannot be a redirect char
  for (; index < cmd->size; index++) {
    if (!strcmp(cmd->start[index], trigger)) {
      if (index + 2 == cmd->size || // at then end
          (cmd->size > index + 2 && !strcmp(cmd->start[index + 2], after)))
      // a proceding term is after
      {
        *file = (char *)pvec_pop_nth(cmd, index + 1);
        free(pvec_pop_nth(cmd, index));
        return 1;
      } else {
        return -1;
      }
    }
  }
  return 0;
}

int pipe_p(char *maybe_pipe) {
  return maybe_pipe != NULL && !strcmp(maybe_pipe, "|");
}

// Exectues an non-builtin command. `cmd` is the parsed command to be executed.
// `path`, `action`, and `processes` are treated the same as `exec_command`.
void exec_external(PVec *cmd, PVec *path, enum ACTION *signal,
                   PVec *processes) {
  char *absolute_path = resolve_command(cmd->start[0], path);
  char *pipe_to, *pipe_from;
  PVec pipe_cmd = pvec_split(cmd, (int (*)(void *)) & pipe_p, 0);
  char *pipe_absolute_path =
      pipe_cmd.size ? resolve_path(pipe_cmd.start[0], path) : NULL;
  if (pipe_absolute_path != NULL) {
    free(pipe_cmd.start[0]);
    pipe_cmd.start[0] = pipe_absolute_path;
  }
  char *pipe_rest = pvec_concat(&pipe_cmd, " ");
  int handle_output_res;
  if (absolute_path != NULL) {
    pid_t pd = fork();
    if (pd == 0) {
      // If errors are found in the fork, always throw hard errors.
      if (pipe_cmd.size > 0) {
        FILE *out = popen(pipe_rest, "w");
        fclose(stdout);
        dup2(fileno(out), STDOUT_FILENO);
      }
      if (pipe_cmd.size == 0)
        // handle the '>' operator
        handle_output_res = handle_redirect(cmd, &pipe_to, ">", "<");
      else
        handle_output_res = 0;
      pvec_free(&pipe_cmd);
      int handle_input_res = handle_redirect(cmd, &pipe_from, "<", ">");
      if (handle_input_res > 0) {
        FILE *in = fopen(pipe_from, "r");
        if (in == NULL) {
          free(pipe_from);
          signal_error(in == NULL, 1, NULL);
        } else {
          fclose(stdin);
          dup2(fileno(in), STDIN_FILENO);
        }
        free(pipe_from);
      } else if (handle_input_res < 0) {
        signal_error(1, 1, NULL);
      }

      if (handle_output_res > 0) {
        fclose(stdout);
        fopen(pipe_to, "w");
        free(pipe_to);
      } else if (handle_output_res < 0) {
        signal_error(1, 1, NULL);
      }
      pvec_push(cmd, NULL);
      execv(absolute_path, (char **)cmd->start);
    } else if (pd > 0) {
      pvec_free(&pipe_cmd);
      int status;
      pvec_push(processes,
                (void *)(long)
                    pd); // NOTE: confuses valgrind.
                         // Causes valgrind to think there is a leak of 8 bytes.
      waitpid(pd, &status, *signal == ASYNC);
    } else {
      // pd < 0
      signal_error(1, 0, NULL);
    }
    free(absolute_path);
  } else {
    errno = ENOENT;
    signal_error(1, 0, ERROR_MESSAGE("Could not find command in path"));
  }
  free(pipe_rest);
}

// `cmd` is a the parsed command instruction. Calling exec_command gives
// ownership of `cmd` to the callee. `path` is the exec-path for the shell. It
// can be mutated but will end valid. `action` represents the current action
// state, and can be mutated. `processes` is a list of processes left running,
// and contains not pointers but integers. Processes can be added.
void exec_command(PVec *cmd, PVec *path, enum ACTION *action, PVec *processes) {
  if (!cmd->size || *action == EXIT) {
    pvec_free(cmd);
    return;
  }
  char *first = cmd->start[0];
  if (!strcmp(first, CD_COMMAND)) {
    errno = E2BIG;
    signal_error(cmd->size != 2, 0,
                 ERROR_MESSAGE("The \"cd\" command takes 1 argument"));
    chdir(cmd->start[1]);
  } else if (!strcmp(first, EXIT_COMMAND)) {
    *action = EXIT;
    errno = E2BIG;
    signal_error(cmd->size != 1, 0,
                 ERROR_MESSAGE("The \"exit\" command takes 0 arguments."));
  } else if (!strcmp(first, PATH_COMMAND)) {
    size_t index = 1;
    // if the first command is "-+", then don't remove the old path
    if (cmd->size > 1 && !(strcmp(cmd->start[1], SHOW_PATH_COMMAND))) {
      char *concat_path = pvec_concat(path, ":");
      printf("path = \"%s\"\n", concat_path);
      free(concat_path);
      index = cmd->size;
    } else if (cmd->size > 1 && !(strcmp(cmd->start[1], ADD_PATH_COMMAND))) {
      index = 2;
    } else {
      while (pvec_pop(path) != NULL) {
      }
    }
    for (; index < cmd->size; index++) {
      pvec_push(path, cmd->start[index]);
      cmd->start[index] = NULL;
    }
  } else {
    exec_external(cmd, path, action, processes);
  }
  pvec_free(cmd);
}

// Returns PVec containing the local path. If native is != 0, will grab the
// native path from the calling enviromental variable `PATH`. If `PATH` is not
// set, it will signal an error by setting `error` to 1, and set the default
// path. If native == 0, then the default path is set.
PVec get_path(int native, int *error) {
  PVec path = pvec_make();
  if (native) {
    char *native = getenv("PATH"); // NOTE: this convinces valgrind that memory
                                   // was leaked.
    *error = native == NULL;
    if (native != NULL) {
      char *ncopy = malloc(sizeof(char) * (strlen(native) + 1));
      strcpy(ncopy, native);
      char *pvar;
      while ((pvar = strsep(&ncopy, ":")) != NULL) {
        char *pcpy = malloc(sizeof(char) * (strlen(pvar) + 1));
        strcpy(pcpy, pvar);
        pvec_push(&path, pcpy);
      }
      free(ncopy);
    }
  }
  if (!native || *error) {
    char *defpath = malloc(sizeof(char) * (strlen(DEFAULT_PATH) + 1));
    strcpy(defpath, DEFAULT_PATH);
    pvec_push(&path, defpath);
  }
  return path;
}

// Executes the main logic of the shell:
// reading a line, then responding to it.
// input is the input source,
// batch = 1 => batch mode
// batch = 0 => not batch mode
void main_loop(FILE *input, int batch) {
  // setup path
  int path_error = 0;
  PVec path = get_path(1, &path_error);

  // setup getline
  char *line = NULL;
  size_t len = 0;
  ssize_t nread = 0;
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
    int line_end = line ? strlen(line) : 0;
    if (line_end > 0)
      add_history(line);
    int read_to = 0;
    while (action != EXIT && line_end > read_to) {
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
  free(processes.start);
}

int main(int argc, char **argv) {
  if (argc == 1) {
    main_loop(stdin, 0);
  } else if (argc == 2) {
    FILE *file = fopen(argv[1], "r");
    if (file == NULL)
      signal_error(file == NULL, 1,
                   ERROR_MESSAGE("Could not find batch input file"));
    else {
      main_loop(file, 1);
      fclose(file);
    }
  } else {
    errno = EINVAL;
    signal_error(1, 1, ERROR_MESSAGE("usage: wish [file]"));
  }
}
