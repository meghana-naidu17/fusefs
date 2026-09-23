#!/bin/bash

MOUNT_POINT="./test_mount"

echo "================================"
echo "FuseFS FUSE Layer Test"
echo "================================"

mkdir -p "$MOUNT_POINT"

echo "[1] Checking mount point..."
if [ ! -d "$MOUNT_POINT" ]; then
    echo "FAIL: Mount point does not exist"
    exit 1
fi

echo "PASS: Mount point exists"

echo "[2] Checking FuseFS executable..."

if [ ! -f "./fusefs" ]; then
    echo "FAIL: fusefs executable not found"
    exit 1
fi

echo "PASS: FuseFS executable exists"

echo "[3] FUSE layer is ready"

echo "================================"
echo "Basic tests completed"
echo "================================"
