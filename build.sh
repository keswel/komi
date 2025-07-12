#!/bin/bash

# Find PID using port 8080 and kill it if exists
PID=$(lsof -t -i:8080)
if [ -n "$PID" ]; then
    echo "Killing process on port 8080 (PID $PID)..."
    kill -9 $PID
fi

# compile programs
g++ komi.cpp -o komi -lraylib -lGL -lm -lpthread -ldl -lrt 
g++ server.cpp -o server -lboost_system

# start server in background
./server &
SERVER_PID=$!

sleep 1  # wait for server to start

# run client
./komi

# kill server process after client exits
kill $SERVER_PID
