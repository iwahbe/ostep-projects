#include <assert.h>
#include <fcntl.h>
#include <pthread/pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_pthread/_pthread_attr_t.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/_types/_size_t.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

const int INT_OFFSET = 4;

const int DEBUG_INFO = 0;
#define eprintf(...)                                                           \
  if (DEBUG_INFO)                                                              \
  fprintf(stderr, __VA_ARGS__)

const int CHUNK_SIZE = 5;
const int THREAD_BUFF_LENGTH = CHUNK_SIZE * 5;

const char *NTHREADS = "NTHREADS";

typedef struct {
  int write_num;
  pthread_mutex_t guard;
} WriteHead;

WriteHead WRITE_HEAD;

/*
 * Setup write-head.
 */
void write_head_init(WriteHead *write_head) {
  write_head->write_num = 1;
  pthread_mutex_init(&write_head->guard, NULL);
}

/*
 * Locks the write-head, returning a pointer to the write position.
 * This pointer should be updated on write.
 */
void write_head_lock(WriteHead *head) { pthread_mutex_lock(&head->guard); }

void write_head_unlock(WriteHead *head) { pthread_mutex_unlock(&head->guard); }

int write_head_num(WriteHead *head) { return head->write_num; }

/*
 * Writes the buff to head.
 * Assumes that the write-head is already locked.
 */
void write_head_write_buff(WriteHead *head, unsigned char *buff, int length) {
  if (length > 0)
    fwrite(buff, length, 1, stdout);
  head->write_num++;
  eprintf("Written %d chars.\n", length);
}

typedef struct {
  char last_char;
  int count;
} CompressInfo;

void compress_info_init(CompressInfo *info) {
  info->last_char = EOF;
  info->count = 0;
}

typedef struct {
  int tasknum;          // The sequential ordering of writes, starting from 1.
  char *read_begin;     // The begininning of the sequence to encode.
  char *read_end;       // The end of the sequence to decode.
  CompressInfo info;    // Information about the decoding taking place.
  int exit_immediately; // Whither to exit after finishing one cycle.
  int write_end; // Whither to write the end, or wait for new input to encode.
  int return_tasknum; // The tasknum to next write to.
  int buff_index;
  unsigned char buff[THREAD_BUFF_LENGTH];
} TaskDescriptor;

void task_descriptor_init(TaskDescriptor *task) {
  task->tasknum = 0;
  task->read_end = NULL;
  task->read_begin = NULL;
  task->exit_immediately = 0;
  task->write_end = 0;
  task->buff_index = 0;
  task->return_tasknum = -1;
  compress_info_init(&task->info);
}
void eprint_write_buff(unsigned char *buf, int length) {
  eprintf("buf: '");
  for (int len = 0; len < length; len += INT_OFFSET + 1) {
    int n = (*(buf + len) << 0) + (*(buf + len + 1) << 8) +
            (*(buf + len + 2) << 16) + (*(buf + len + 3) << 24);
    eprintf("%d%c", n, *(buf + len + INT_OFFSET));
  }
  eprintf("'\n");
}

void write_internal_buff(unsigned char *buff, int *index, char c, int count) {
  if (count > 0) {
    unsigned char *ind = buff + *index;
    *index += INT_OFFSET + 1;
    ind[3] = (count >> 24) & 0xFF;
    ind[2] = (count >> 16) & 0xFF;
    ind[1] = (count >> 8) & 0xFF;
    ind[0] = count & 0xFF;
    ind[4] = c;
    eprintf("Writing '%d%c'\n", count, c == '\n' ? '@' : c);
  }
}

void sync_write_from_internal_buff(TaskDescriptor *desc) {
  int do_loop = 1;
  while (do_loop) {
    write_head_lock(&WRITE_HEAD); // TODO: streamline concurency
    assert(desc->tasknum >= write_head_num(&WRITE_HEAD));
    if (desc->tasknum == write_head_num(&WRITE_HEAD)) {
      if (!desc->write_end) {
        eprint_write_buff(desc->buff, desc->buff_index);
        write_head_write_buff(&WRITE_HEAD, desc->buff, desc->buff_index);
        desc->buff_index = 0;
      } else {
        desc->return_tasknum = desc->tasknum;
      }
      do_loop = 0;
      desc->tasknum = 0;
    }
    write_head_unlock(&WRITE_HEAD);
  }
}
// reads the range described by desc into buff, returning the length read of the
// buff read.
void read_to_internal_buff(TaskDescriptor *desc) {
  char c = EOF;
  while (desc->read_end > desc->read_begin) {
    c = *((desc->read_begin)++);
    if (c == desc->info.last_char) {
      (desc->info.count)++;
    } else {
      write_internal_buff(desc->buff, &desc->buff_index, desc->info.last_char,
                          desc->info.count);
      desc->info.last_char = c;
      desc->info.count = 1;
    }
  }
  if (desc->write_end == 0) {
    write_internal_buff(desc->buff, &desc->buff_index, desc->info.last_char,
                        desc->info.count);
    desc->info.last_char = EOF;
    desc->info.count = 0;
  }
}

void write_section(TaskDescriptor *desc) {
  do {
    if (desc->tasknum) { // TODO: put in non spin-lock here
      eprintf("executing task_num=%d with write_end=%d\n", desc->tasknum,
              desc->write_end);
      read_to_internal_buff(desc);
      sync_write_from_internal_buff(desc);
    }
  } while (!desc->exit_immediately);
}

// mmap a file into memeory,
// setting file to the file,
// setting length to the file length
// returning 0 if successful
int mmap_file(char *file_name, char **file, size_t *length) {
  struct stat sb;
  int fd = open(file_name, O_RDONLY);
  if (fd == -1)
    return 1;
  int fstat_code = fstat(fd, &sb);
  if (fstat_code == -1)
    return 2;
  *file = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (*file == MAP_FAILED)
    return 3;
  *length = sb.st_size;
  return 0;
}

// returns if the batch was the last in the file.
int set_next_task(int *task_num, TaskDescriptor *task, char **head, char *end,
                  int use_pending_info, CompressInfo pending_info) {
  if (*head >= end) {
    return 1;
  }
  task->read_begin = *head;
  char *seek = *head + CHUNK_SIZE;
  // don't seek past the then
  if (end < seek)
    seek = end;
  // don't break a section into diffrent tasks
  while (seek + 1 < end && *seek == *(seek + 1))
    seek++;
  if (seek < end)
    seek++;
  eprintf("head landed on '%c'\n", *seek != '\n' ? *seek : '@');
  *head = (task->read_end = seek);
  int incr_task_num = !task->write_end; // from old run
  task->write_end = task->read_end == end;
  int out = task->read_end == end;
  // This triggers action, so must happen last
  int tasknum = (*(task_num) += incr_task_num);
  if (use_pending_info)
    task->info = pending_info;
  task->tasknum = tasknum;
  eprintf("Head set to offset %ld on task %d\n", (long)(end - *head), tasknum);
  return out;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    printf("pzip: file1 [file2 ...]\n");
    return 1;
  } else {
    // Note: the main thread handles task[0]
    // SETUP HEAD
    write_head_init(&WRITE_HEAD);

    // GET THREAD COUNT
    char *given_thread_count = getenv(NTHREADS);
    int concurrent_task_num = given_thread_count ? atoi(given_thread_count) : 1;
    if (concurrent_task_num < 1) {
      printf("%s must be a positive integer\n", NTHREADS);
      exit(1);
    }
    assert(concurrent_task_num == 1);

    // INITIALIZE TASKS
    TaskDescriptor *tasks =
        (TaskDescriptor *)malloc(sizeof(TaskDescriptor) * concurrent_task_num);
    if (tasks == NULL)
      return 1;
    for (int i = 0; i < concurrent_task_num; i++) {
      task_descriptor_init(tasks + i);
    }
    tasks->exit_immediately = 1;

    // INITIALIZE THREADS
    pthread_t *pinfo;
    pinfo = (pthread_t *)malloc(sizeof(pthread_t) * concurrent_task_num);
    if (pinfo == NULL)
      return 1;
    pthread_attr_t pattr;
    pthread_attr_init(&pattr);
    for (int i = 1; i < concurrent_task_num; i++) {
      if (!pthread_create(pinfo + i, &pattr, (void *)write_section, tasks + i))
        return 1;
    }

    int last_task = 0; // -1 means info is pending, otherwise it indexes into
                       // the appropriate task.
    CompressInfo pending_info;

    // PROCESS FILES
    int task_num = 0;
    for (int i = 1; i < argc; i++) {
      char *file, *findex, *end;
      size_t length;

      if (mmap_file(argv[i], &file, &length))
        return 1;
      findex = file;
      end = file + length;
      int exit = 0;
      // file is not yet processed
      while (end > findex && !exit) {
        for (int task_i = 0; task_i < concurrent_task_num; task_i++) {
          if (tasks[task_i].tasknum == 0) {
            exit = set_next_task(&task_num, tasks + task_i, &findex, end,
                                 last_task == -1, pending_info);
            if (exit) {
              last_task = task_i;
            }
          }
        }
        write_section(tasks + 0); // this one returns on completion
      }
      munmap(file, length);
      // TODO: this better be done executing when it is saved.
      pending_info = tasks[last_task].info; // save the info of the last task.
      last_task = -1;
    }
    // Finish when done
    for (int i = 0; i < concurrent_task_num; i++) {
      tasks[i].tasknum = tasks[i].write_end ? tasks[i].return_tasknum : 0;
      tasks[i].write_end = 0;
      tasks[i].exit_immediately = 1;
    }
    // Join
    write_section(tasks + 0); // this one returns on completion
    for (int i = 1; i < concurrent_task_num; i++)
      pthread_join(pinfo[i], NULL);
    // cleanup
    free(pinfo);
    free(tasks);
  }
}
