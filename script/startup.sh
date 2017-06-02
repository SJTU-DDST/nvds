#!/bin/bash
../src/build/coordinator ./config.json > coordinator.txt &
# Waiting for coordinator startup
sleep 0.1s

for ((i = 5050; i < 5050 + $1; i = i + 1)) do
    ../src/build/server $i ./config.json > "server_$i.txt" &
    sleep 0.1s
done
