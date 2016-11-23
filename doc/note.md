# My Notes

## Plan
1. 先做2个节点之间的通信与query，一开始可先不做复制；测试单一节点的QPS；
2. 考虑持久化的设计；
3. 将其拓展到分布式结构；
4. 再去考虑故障检测，故障恢复等问题；

## RDMA
1. [libvma](https://github.com/Mellanox/libvma): Enable standard socket API over Ethernet or Infiniband  from user-space with full network stack bypass.

2. rdma_cma.h: Used to establish communication over RDMA transports.

## RDMA 理解
1. Channel Adapter: 其实就是指NIC
2. RDMA存在两种典型的semantics: memory semantic 和 channel semantic；memory semantic就是由发起方指定读写的地址，完全不需要host的cpu参与， 这称为one-sided；这对应于RDMA的READ、WRITE；channel semantic对应于RDMA的SEND、RECV

## 测试

### RDMA 延迟
相同机架上两台机器之间的RDMA写延迟为1.21us，读延迟为1.20us；读写数据长度为64Byte；