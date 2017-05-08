#!/bin/bash
../src/build/coordinator ./config.json &
# Waiting for coordinator startup
sleep 1s
./startup_servers.sh
