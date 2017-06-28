#!/bin/bash

../src/build/coordinator > coordinator.txt 2> err_coordinator.txt < /dev/null &

# Waiting for coordinator
sleep 0.1s

first_line=$(head -n 1 coordinator.txt)
first_line_arr=($first_line)

# Compatible with the server's bash
#coord_addr=${first_line_arr[-1]}
coord_addr=${first_line_arr[2]}

while read line; do
    server=($line)
    echo "starting server [${server[0]}:${server[1]}]"
    dir="/home/wgtdkp/nvds/"
    bin="./src/build/server"
    out="./script/server_${server[1]}.txt"
    err="./script/err_server_${server[1]}.txt"
    in="/dev/null"
    ssh root@${server[0]} "cd ${dir}; nohup ${bin} ${server[1]} ${coord_addr} >> ${out} 2>> ${err} < ${in} &" < /dev/null
done < servers.txt
