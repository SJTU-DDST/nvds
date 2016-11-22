#include "infiniband.h"

void nvds_get_device() {
  int n = 0;
  struct ibv_device** ib_dev_list;
  ib_dev_list = ibv_get_device_list(&n);
  if (ib_dev_list == NULL) {
    nvds_error("no available infiniband devices!");
  } else {
    printf("found %d infiniband device\n", n);
  }

  struct ibv_device* ib_dev = ib_dev_list[0];
  if (ib_dev) {}
}
