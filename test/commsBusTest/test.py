import os
import sys
import paramiko
import subprocess
import shutil

isWin = sys.platform == "win32"

def IsDriveExists(drive):
    return os.path.exists(drive + ':\\')

# is the drive mapped?
if IsDriveExists("z"):
    print("Haiku drive mapped...")
else:
    print("Haiku drive not mapped...")
    root = "C:\\codelib\\sshfs-win\\.build\\x64\\root\\bin"
    cmd = [os.path.join(root, "sshfs-win"), "svc", "\\sshfs.k\\user@haiku", "z:"] 
    print("running:", " ".join(cmd))
    p = subprocess.Popen(cmd, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.PIPE)
    os.set_blocking(p.stdout.fileno(), False)
    while p.poll() == None:
        out = p.stdout.read()
        if out != None:
            print("out:", out.decode())
    print("ssh-fs loop done")

# copy the code across
relPath = "src/common/Net/CommsBus.cpp"
remotePath = "code/lgi/trunk"
src = os.path.abspath(os.path.join(__file__, "../../..", relPath))
dst = "z:/" + remotePath + "/" + relPath
shutil.copyfile(src, dst)
print("cpy src:", src, "exists:", os.path.exists(src))

# build the software in the VM
client = paramiko.SSHClient()
client.load_system_host_keys()
client.connect("haiku", 22, "user")
buildDir = remotePath + "/test/commsBusTest/build"
(stdin, stdout, stderr) = client.exec_command("cd " + buildDir + " && make -j 8")
output = stdout.read()
print(str(output, 'utf8'))

# run the software in the VM
print("running...")
(stdin, stdout, stderr) = client.exec_command("cd " + buildDir + " && ./commsBusTest")
print("reading...")
while True:
    print(stdout.read(1).decode(), end='')
