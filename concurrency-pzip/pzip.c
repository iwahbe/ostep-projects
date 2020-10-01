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

const int CHUNK_SIZE = 1000;
const int THREAD_BUFF_LENGTH = CHUNK_SIZE * 5;

const char *NTHREADS = "NTHREADS";

typedef struct {
  int write_num;
  pthread_mutex_t guard;
} WriteHead;

WriteHead WRITE_HEAD;

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
void write_head_write_buff(WriteHead *head, char *buff, int length) {
  fwrite(buff, length, 1, stdout);
  head->write_num++;
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
  int tasknum;
  char *read_begin;
  char *read_end;
  CompressInfo info;
  int exit_immediately;
} TaskDescriptor;

void task_descriptor_init(TaskDescriptor *task) {
  task->tasknum = 0;
  task->read_end = NULL;
  task->read_begin = NULL;
  task->exit_immediately = 0;
  compress_info_init(&task->info);
}

void write_section(TaskDescriptor *desc) {
  char buff[THREAD_BUFF_LENGTH];
  int buff_index = 0;
  do {
    printf("considering task_num=%d\n", desc->tasknum);
    if (desc->tasknum) { // TODO: put in non spin-lock here
      printf("executing task_num=%d\n", desc->tasknum);
      char c;
      buff_index = 0;
      while (desc->read_end > desc->read_begin &&
             (c = *((desc->read_begin)++))) {
        if (c == desc->info.last_char) {
          (desc->info.count)++;
        } else {
          if (desc->info.count) {
            buff[buff_index] = desc->info.count;
            buff_index += 4;
            buff[buff_index++] = c;
          }
          desc->info.last_char = c;
          desc->info.count = 1;
        }
      }
      printf("finished read loop on task_num=%d\n", desc->tasknum);
      int do_loop = 1;
      while (do_loop) {
        write_head_lock(&WRITE_HEAD); // TODO: streamline concurency
        if (desc->tasknum == write_head_num(&WRITE_HEAD)) {
          write_head_write_buff(&WRITE_HEAD, buff, buff_index);
          do_loop = 0;
          desc->tasknum = 0;
        }
        write_head_unlock(&WRITE_HEAD);
      }
      printf("finished write loop on task_num=%d\n", desc->tasknum);
    }
  } while (!desc->exit_immediately);
}

void write_compressed_char(FILE *to, char c, int *count) {
  if (*count > 0) {
    fwrite(count, 4, 1, to);
    putc(c, to);
    *count = 0;
  }
}

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

void set_next_task(int task_num, TaskDescriptor *task, char **head, char *end) {
  if (*head >= end)
    return;
  task->read_begin = *head;
  char *seek = *head + CHUNK_SIZE;
  // don't seek past the then
  if (end < seek)
    seek = end;
  // don't break a section into diffrent tasks
  while (seek + 1 < end && *seek == *(seek + 1))
    seek++;
  *head = (task->read_end = seek);
  // This triggers action, so must happen last
  task->tasknum = task_num;
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

    // PROCESS FILES
    int task_num = 0;
    for (int i = 1; i < argc; i++) {
      char *file, *findex, *end;
      size_t length;

      if (mmap_file(argv[i], &file, &length))
        return 1;
      findex = file;
      end = file + length;
      printf("File process started\n");
      // file is not yet processed
      while (end > findex) {
        for (int i = 0; i < concurrent_task_num; i++) {
          if (tasks[i].tasknum == 0) {
            set_next_task(++task_num, tasks + i, &findex, end);
            printf("Task set for task_num=%d\n", task_num);
          }
        }
        printf("task dispatch done, on task_num=%d\n", task_num);
        write_section(tasks + 0); // this one returns on completion
        printf("main thread writen on task_num=%d\n", task_num);
      }
      printf("loop exited\n");
      munmap(file, length);
      printf("One cycle done\n");
    }
    for (int i = 1; i < concurrent_task_num; i++)
      pthread_join(pinfo[i], NULL);
    free(pinfo);
    free(tasks);
  }
}
