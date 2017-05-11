#!/bin/bash
for ((i = 5050; i < 5050 + $1; i = i + 1)) do
    ../src/build/server $i ./config.json > "server_$i.txt" &
    sleep 0.1s
done
