
[Google Test]: https://github.com/google/googletest
[json]: https://github.com/nlohmann/json

# NVDS

Fast replication over RDMA fabric for NVM-based storage system.

## DEPENDECY

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

