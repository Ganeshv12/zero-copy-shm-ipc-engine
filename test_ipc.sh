#!/bin/bash

# Simple automated test harness for Zero-Copy IPC Engine
echo "[Test] Cleaning and building binaries..."
make clean && make

# Start server in the background
echo "[Test] Starting ipc_server daemon..."
./ipc_server &
SERVER_PID=$!

# Wait a second for shared memory to initialize
sleep 1

echo "[Test] Launching concurrent client processes..."
./ipc_client "Test Packet Alpha: Sensor 1 online" &
./ipc_client "Test Packet Bravo: Battery level 98%" &
./ipc_client "Test Packet Charlie: Critical override sequence initiated" &

# Wait for clients to finish writing
sleep 2

# Shut down the server gracefully using SIGINT
echo "[Test] Sending shutdown signal to server (PID: $SERVER_PID)..."
kill -INT $SERVER_PID

# Wait for server process to terminate
wait $SERVER_PID 2>/dev/null

echo "[Test] Automation run complete."
