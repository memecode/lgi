#!/usr/bin/env python3
import sys
import os
import subprocess
from difflib import SequenceMatcher

if len(sys.argv) != 2:
    print("Error: needs 1 arg")
    sys.exit(-1)

exe = sys.argv[1]
if not os.path.exists(exe):
    print("Error: exe", exe, "doesn't exist")
    sys.exit(-2)

p = subprocess.run(["otool", "-L", exe], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
lines = p.stdout.decode().split("\n")
for ln in lines:
    if ln.find("@rpath") >= 0:
        oldLib = ln.split()[0]
        newLib = oldLib.replace("@rpath", "@executable_path/../Frameworks")

        # Look at the final path and check if it exists...
        fullNew = os.path.abspath( newLib.replace("@executable_path", os.path.dirname(exe)) )
        if not os.path.exists(fullNew):
            # It doesn't but most like the lib name is slightly different to the linker name
            files = os.listdir(os.path.dirname(fullNew))
            found = None
            for file in files:
                match = SequenceMatcher(None, file, os.path.basename(newLib))
                if match.ratio() > 0.8:
                    found = file
            if not found is None:
                newLib = "@executable_path/../Frameworks/" + found
            else:
                print("Error: No matching lib for", fullNew, files)
                sys.exit(-3)

        print(oldLib, "->", newLib)
        p = subprocess.run(["install_name_tool", "-change", oldLib, newLib, exe], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if p.returncode != 0:
            print("Error:", p.stdout.decode())
            sys.exit(-4)        

p = subprocess.run(["otool", "-L", exe], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
print("Final:", p.stdout.decode())
