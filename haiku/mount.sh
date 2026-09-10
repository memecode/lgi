#!/usr/bin/env bash

mkdir -p ./remote_dir
sshfs -p 2222 -o follow_symlinks user@localhost:/../.. ./remote_dir
