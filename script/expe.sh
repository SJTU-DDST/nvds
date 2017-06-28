#!/bin/bash

coord="192.168.99.14"
client="192.168.99.17"
for ((len = 16; len <= 1024; len = len * 2)); do
    for ((i = 0; i < 10; i = i + 1)); do
        ./stop.sh
        ./startup.sh
        dir="/home/wgtdkp/nvds/"
        bin="./src/build/client"
        in="/dev/null"
        
        sleep 0.2
        ssh root@${client} "cd ${dir}; nohup ${bin} ${coord} 10000 ${len} 1 >> client.txt < ${in} &" < /dev/null
        sleep 0.3
        pkill -SIGINT server
        #cat server_5050.txt
        sleep 0.1
    done
done
