#!/usr/bin/env bash

mkdir -p ./remote_dir
sshfs -p 2200 -o follow_symlinks user@localhost:/../.. ./remote_dir
