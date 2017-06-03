#!/bin/bash

../src/build/coordinator > coordinator.txt &

# Waiting for coordinator
sleep 0.1s

first_line=$(head -n 1 coordinator.txt)
coord_addr=($first_line)[-1]

while read line; do
    server=($line)
    ssh root@${server[0]} '/home/wgtdkp/nvds/build/server ${server[1]} ${coord_addr} > server_${server[1]}.txt &'
done < servers.txt
