#!/bin/bash

MOUNT_POINT="./fusefs_mount"

mkdir -p "$MOUNT_POINT"

echo "Starting FuseFS..."

./fusefs -f "$MOUNT_POINT"
