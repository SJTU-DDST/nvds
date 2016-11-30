#ifndef _NVDS_NVDS_H_
#define _NVDS_NVDS_H_

#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <sys/time.h>
// Interprocess communication access
#include <sys/ipc.h>
// Shared memory facility
#include <sys/shm.h>

namespace nvds {

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

}

#endif // _NVDS_NVDS_H_
