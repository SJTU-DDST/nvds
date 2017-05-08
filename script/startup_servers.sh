#!/bin/bash
../src/build/server 5050 ./config.json > server_5050.txt &
sleep 0.1s
../src/build/server 6060 ./config.json > server_6060.txt &
sleep 0.1s
../src/build/server 7070 ./config.json > server_7070.txt &
