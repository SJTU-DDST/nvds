#!/bin/bash
killall -9 -q coordinator

while read line; do
    server=($line)
    ssh root@${server[0]} 'killall -9 -q server'
done < servers.txt
