#ifndef NVDS_NVDS_H_
#define NVDS_NVDS_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <memory.h>

#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
// Interprocess communication access
#include <sys/ipc.h>
// Shared memory facility
#include <sys/shm.h>

#define nvds_error(format, ...) {                         \
  fprintf(stderr, "error: %s: %d: ", __FILE__, __LINE__); \
  fprintf(stderr, format, ## __VA_ARGS__);                \
  fprintf(stderr, "\n");                                  \
  exit(-1);                                               \
}

#define nvds_expect(cond, err, ...) { \
  if (!(cond)) {                      \
    nvds_error(err, ## __VA_ARGS__);  \
  }                                   \
}

#endif
