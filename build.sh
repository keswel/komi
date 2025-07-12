#!/bin/bash

g++ komi.cpp -o komi -lraylib -lGL -lm -lpthread -ldl -lrt 
g++ server.cpp -o server -lboost_system

./server &              # Start server in background
SERVER_PID=$!

sleep 1                 # Wait briefly to ensure the server is ready

./komi                  # Run main program

kill $SERVER_PID        # Clean up server process
