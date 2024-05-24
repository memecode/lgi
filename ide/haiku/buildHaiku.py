#!/usr/bin/env python3
import os
import sys
import paramiko
import re

ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
print("cwd:", os.getcwd())
password = open("haikuPassword.txt", "r").read().strip()
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect("192.168.0.205", 22, "user", password)

def cmd(c):
	global ssh
	stdin, stdout, stderr = ssh.exec_command(c, get_pty=True)
	while True:
		ln = stdout.readline()
		if len(ln) > 0:
			s = ansi_escape.sub('', ln[0:-1])
			print(s, flush=True)
		else:
			break

cmd("cd code/lgi/trunk/ide && make -j8 -f haiku/Makefile.haiku 2>&1")
