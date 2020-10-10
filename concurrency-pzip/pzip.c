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

#ifndef DEBUG_INFO
const unsigned int __DEBUG_INFO = 0;
#define DEBUG_INFO
#endif

#define eprintf(priority, ...)                                                 \
  if (priority <= __DEBUG_INFO)                                                \
  fprintf(stdout, __VA_ARGS__)

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
void write_head_init() {
  WRITE_HEAD.write_num = 1;
  pthread_mutex_init(&WRITE_HEAD.guard, NULL);
}

/*
 * Locks the write-head, returning a pointer to the write position.
 * This pointer should be updated on write.
 */
void write_head_lock() {
  pthread_mutex_lock(&WRITE_HEAD.guard);
  eprintf(3, "WRITE_HEAD locked\n");
}

void write_head_unlock() {
  pthread_mutex_unlock(&WRITE_HEAD.guard);
  eprintf(3, "WRITE_HEAD unlocked\n");
}

int write_head_num() { return WRITE_HEAD.write_num; }

/*
 * Writes the buff to head.
 * Assumes that the write-head is already locked.
 */
void write_head_write_buff(unsigned char *buff, int length) {
  if (length > 0)
    fwrite(buff, length, 1, stdout);
  WRITE_HEAD.write_num++;
  eprintf(3, "Written %d chars.\n", length);
}

typedef struct {
  char last_char;
  int count;
  int buff_index;
  unsigned char buff[THREAD_BUFF_LENGTH];
} CompressInfo;

void compress_info_init(CompressInfo *info) {
  info->last_char = EOF;
  info->count = 0;
  info->buff_index = 0;
}

typedef struct {
  int tasknum;          // The sequential ordering of writes, starting from 1.
  char *read_begin;     // The begininning of the sequence to encode.
  char *read_end;       // The end of the sequence to decode.
  CompressInfo info;    // Information about the decoding taking place.
  int exit_immediately; // Whither to exit after finishing one cycle.
  int write_end; // Whither to write the end, or wait for new input to encode.
  int return_tasknum; // The tasknum to next write to.
  pthread_t thread;
} TaskDescriptor;

void task_descriptor_init(TaskDescriptor *task) {
  task->tasknum = 0;
  task->read_end = NULL;
  task->read_begin = NULL;
  task->exit_immediately = 0;
  task->write_end = 0;
  task->return_tasknum = -1;
  compress_info_init(&task->info);
}
void eprint_write_buff(unsigned char *buf, int length) {
  eprintf(3, "buf: '");
  for (int len = 0; len < length; len += INT_OFFSET + 1) {
    int n = (*(buf + len) << 0) + (*(buf + len + 1) << 8) +
            (*(buf + len + 2) << 16) + (*(buf + len + 3) << 24);
    eprintf(3, "%d%c", n, *(buf + len + INT_OFFSET));
  }
  eprintf(3, "'\n");
}

void write_internal_buff(unsigned char *buff, int *index, char c, int count) {
  if (count > 0) {
    if (*index > 1 && buff[*index - 1] == c) {
      count = count + (buff[*index - 2] << 24 & 0xFF) +
              (buff[*index - 3] << 16 & 0xFF) + (buff[*index - 4] << 8 & 0xFF) +
              (buff[*index - 5] << 0 & 0xFF);
      *index -= 5;
    }
    unsigned char *ind = buff + *index;
    *index += INT_OFFSET + 1;
    ind[3] = (count >> 24) & 0xFF;
    ind[2] = (count >> 16) & 0xFF;
    ind[1] = (count >> 8) & 0xFF;
    ind[0] = count & 0xFF;
    ind[4] = c;
    eprintf(4, "Writing internal '%d%c'\n", count, c == '\n' ? '@' : c);
  }
}

void sync_write_from_internal_buff(TaskDescriptor *desc) {
  int do_loop = 1;
  while (do_loop) {
    write_head_lock(); // TODO: streamline concurency
    assert(desc->tasknum >= write_head_num());
    eprintf(3, "Attempting do-loop of task %d on write_head %d\n",
            desc->tasknum, write_head_num());
    if (desc->tasknum == write_head_num()) {
      if (!desc->write_end) {
        eprint_write_buff(desc->info.buff, desc->info.buff_index);
        write_head_write_buff(desc->info.buff, desc->info.buff_index);
        desc->info.buff_index = 0;
      } else {
        desc->return_tasknum = desc->tasknum;
      }
      do_loop = 0;
      desc->tasknum = 0;
    } else
      eprintf(5, "no action loop: tasknum %d on headnum %d for thread %lu\n",
              desc->tasknum, write_head_num(), (long)desc->thread);
    write_head_unlock();
    eprintf(3, "spin baby spin\n");
  }
  assert(!desc->tasknum);
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
      write_internal_buff(desc->info.buff, &desc->info.buff_index,
                          desc->info.last_char, desc->info.count);
      desc->info.last_char = c;
      desc->info.count = 1;
    }
  }
  write_internal_buff(desc->info.buff, &desc->info.buff_index,
                      desc->info.last_char, desc->info.count);
  desc->info.last_char = EOF;
  desc->info.count = 0;
}

void write_section(TaskDescriptor *desc) {
  do {
    assert(desc->exit_immediately == 0 || desc->exit_immediately == 1);
    if (desc->tasknum) { // TODO: put in non spin-lock here
      eprintf(2, "executing task_num=%d with write_end=%d on thread %lu\n",
              desc->tasknum, desc->write_end, (long)desc->thread);
      read_to_internal_buff(desc);

      eprintf(2, "finished read task_num=%d with write_end=%d on thread %lu\n",
              desc->tasknum, desc->write_end, (long)desc->thread);
      sync_write_from_internal_buff(desc);

      eprintf(2, "finished write task_num=%d with write_end=%d on thread %lu\n",
              desc->tasknum, desc->write_end, (long)desc->thread);
      assert(desc->tasknum == 0);
    }
  } while (!desc->exit_immediately);
  assert(desc->exit_immediately);
  assert(!desc->tasknum);
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
                  int use_pending_info, CompressInfo *pending_info) {
  if (*head >= end) {
    return 0;
  }
  task->exit_immediately =
      (task->thread == 0); // TODO: this shouldn't be necessary
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
  *head = (task->read_end = seek);
  int incr_task_num = !task->write_end; // from old run
  task->write_end = task->read_end == end;
  int out = task->read_end == end;
  // This triggers action, so must happen last
  int tasknum = (*(task_num) += incr_task_num);
  if (use_pending_info) {
    task->info = *pending_info;
    eprintf(3, "Info switch made to buff with length %d\n",
            pending_info->buff_index);
  }
  eprintf(3, "Task assigned to thread %d=%lu with", tasknum,
          (long)task->thread);
  eprintf(3, " head set to offset %ld\n", (long)(end - *head));
  task->tasknum = tasknum;
  return out;
}

int process_files(char **fnames, int flength, TaskDescriptor *tasks,
                  int ntasks) {

  int last_task = 0; // -1 means info is pending, otherwise it indexes into
                     // the appropriate task.
  CompressInfo pending_info;

  for (int i = 0; i < ntasks; i++)
    assert(tasks[i].exit_immediately == 0 || tasks[i].exit_immediately == 1);

  // PROCESS FILES
  int task_num = 0;
  for (int file_i = 1; file_i < flength; file_i++) {
    char *file, *findex, *end;
    size_t length;

    if (mmap_file(fnames[file_i], &file, &length))
      return (1);
    findex = file;
    end = file + length;
    int exit = 0;
    // file is not yet processed
    while (end > findex && !exit) {
      for (int task_i = 0; task_i < ntasks; task_i++) {
        /* assert(tasks[task_i].exit_immediately == 0 || */
        /*        tasks[task_i].exit_immediately == 1); */

        if (tasks[task_i].tasknum == 0) {
          exit = set_next_task(&task_num, tasks + task_i, &findex, end,
                               last_task == -1, &pending_info);
          if (exit) {
            last_task = task_i;
          }
        }
      }
      write_section(tasks + 0); // this one returns on completion
    }
    munmap(file, length);
    while (tasks[last_task].tasknum) {
      write_section(tasks + 0); // NOTE: Spin lock
      eprintf(2,
              "waiting on last_task=%d, "
              "tasknum=%d with headnum=%d on thread %lu, exit_immediately=%d\n",
              last_task, tasks[last_task].tasknum, write_head_num(),
              (long)tasks[last_task].thread, tasks[last_task].exit_immediately);
      eprintf(2,
              "main thread "
              "tasknum=%d with headnum=%d on thread %lu\n",
              tasks[0].tasknum, write_head_num(), (long)tasks[0].thread);
    }
    pending_info = tasks[last_task].info; // save the info of the last task.
    last_task = -1;
  }
  eprintf(1, "Begun cleanup\n");
  // Finish when done
  for (int i = 0; i < ntasks; i++) {
    tasks[i].tasknum = tasks[i].write_end ? tasks[i].return_tasknum : 0;
    tasks[i].write_end = 0;
    tasks[i].exit_immediately = 1;
  }
  eprintf(3, "Joining threads\n");
  // Join
  write_section(tasks + 0); // this one returns on completion
  for (int i = 1; i < ntasks; i++)
    pthread_join(tasks[i].thread, NULL);
  return 0;
}

TaskDescriptor *setup_tasks(int ntasks) {

  // INITIALIZE TASKS
  TaskDescriptor *tasks =
      (TaskDescriptor *)malloc(sizeof(TaskDescriptor) * ntasks + 1);
  assert(tasks != NULL);
  for (int i = 0; i < ntasks; i++) {
    task_descriptor_init(tasks + i);
    assert(tasks[i].exit_immediately == 0);
  }
  tasks[0].exit_immediately = 1;

  // INITIALIZE THREADS
  pthread_attr_t pattr;
  pthread_attr_init(&pattr);
  tasks[0].thread = 0;
  for (int i = 1; i < ntasks; i++) {
    if (pthread_create(&(tasks + i)->thread, &pattr, (void *)write_section,
                       (tasks) + i))
      exit(1);
    assert(tasks[i].exit_immediately == 0 || tasks[i].exit_immediately == 1);
  }
  return tasks;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    printf("pzip: file1 [file2 ...]\n");
    return 1;
  } else {
    // Note: the main thread handles task[0]
    // SETUP HEAD
    write_head_init();

    // GET THREAD COUNT
    char *given_thread_count = getenv(NTHREADS);
    int concurrent_task_num = given_thread_count ? atoi(given_thread_count) : 1;
    if (concurrent_task_num < 1) {
      printf("%s must be a positive integer\n", NTHREADS);
      exit(1);
    }

    TaskDescriptor *tasks = setup_tasks(concurrent_task_num);
    for (int i = 0; i < concurrent_task_num; i++)
      assert(tasks[i].exit_immediately == 0 || tasks[i].exit_immediately == 1);
    // Joins threads, to tasks can now be freed.
    int out = process_files(argv, argc, tasks, concurrent_task_num);

    // cleanup
    free(tasks);
    return out;
  }
}
