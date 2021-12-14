import os
import sys
import paramiko

password = open("haikuPassword.txt", "r").read().strip()
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect("haiku", 22, "user", password)

def cmd(c):
    global ssh
    stdin, stdout, stderr = ssh.exec_command(c)
    out = stdout.readlines() + stderr.readlines()
    print("".join(out).strip())

cmd("cd code/lgi/trunk/Ide && make -j4 2>&1")


