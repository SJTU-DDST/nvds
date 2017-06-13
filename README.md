
[Google Test]: https://github.com/google/googletest
[json]: https://github.com/nlohmann/json

# NVDS

Fast replication over RDMA fabric for NVM-based storage system.

## DEPENDENCY

+ [Google Test]
+ [json]

## BUILD

+ Get the source code:
```shell
$ git clone https://github.com/wgtdkp/nvds --recursive
```

+ Checkout the `master` branch of submodule `json`:
```shell
$ cd nvds/json && git checkout master
```

+ Build nvds from source:
```shell
$ cd ../src && make all
```

## INSTALL
```shell
$ make install # root required
```

## RUN
Navigate to directory `script`:
```shell
script$ ./startup.sh
```

## CONFIGURE
### cluster
Parameters below are defined in header file `common.h`, recompilation and reinstallation are needed for changes to take effect.

| parameter    | default | range | description |
| :--------:   | :---:   | :---: | :---------: |
| kNumReplicas |    1    | [1, ] | the number of replications in primary backup |
| kNumServers  |    2    | [1, ] | the number of servers in this cluster |
| kNumTabletsPerServer | 1 | [1, 16] | the number of tablets per server |

The volume of the whole cluster equals to: kNumServers * kNumTabletsPerServer * 64MB;

### servers
Specify server addresses in file `script/servers.txt`. The total number of servers
should equals to parameter `kNumServers`. The file format: a line for a server address(ip address and port number separated by space). An example of two servers:

```text
192.168.99.14 5050
192.168.99.14 5051
```

## PROGRAMMING
To connect to a nvds cluster, a client only needs to include header file `nvds/client.h` and link nvds's static library.
A tutorial snippet below shows how to put a key/value pair to the cluster and fetch it later:

```c++
#include "nvds/client.h"

#include <iostream>
#include <string>

int main() {
  try {
    nvds::Client c {"192.168.99.14"};
    c.Put("hello", "world");
    std::cout << "hello: " << c.Get("hello") << std::endl;
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (nvds::TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("initialize infiniband devices failed");
  }
  return 0;
}
```

Compile:

```bash
g++ -std=c++11 -Wall test_client.cc -lnvds -lboost_system -lboost_thread -pthread -libverbs
```

The whole code can be obtained in source file `src/test_client.cc`.

Have fun then!
