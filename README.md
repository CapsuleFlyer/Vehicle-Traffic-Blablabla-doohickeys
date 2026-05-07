#!/bin/bash

PROJECT_DIR="/mnt/c/Users/[YOUR USERNAME]/Documents/GitHub/Vehicle Traffic Blablabla doohickeys"

echo "=== Traffic Simulator ==="

# Set DISPLAY for WSL2 (needed for SFML window)
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0
echo "Display set to: $DISPLAY"

# Install SFML if missing
if ! dpkg -s libsfml-dev &>/dev/null 2>&1; then
    echo "Installing SFML..."
    sudo apt-get install -y libsfml-dev g++ > /dev/null 2>&1
fi

echo "Navigating to project folder..."
cd "$PROJECT_DIR" || { echo "ERROR: Folder not found: $PROJECT_DIR"; exit 1; }

echo "Cleaning old build..."
make clean

echo "Compiling..."
make

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Compilation failed. Fix the errors above and try again."
    exit 1
fi

echo ""
echo "Running traffic_simulator..."
echo "========================="
./bin/traffic_simulator
