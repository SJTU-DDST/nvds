#!/bin/bash
../src/build/coordinator ./config.json > coordinator.txt &
# Waiting for coordinator startup
sleep 1s
./startup_servers.sh $1
