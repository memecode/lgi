#!/usr/bin/env python
import os
import sys
import subprocess

frameWorks = sys.argv[1]
if not os.path.exists(frameWorks):
	print("error: no frameworks path:", frameWorks)
	sys.exit(1)

def isUniversal(path):
	key = "(for architecture "
	archs = []
	p = subprocess.run(["file", path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	out = p.stdout.decode()
	for line in out.split("\n"):
		pos = line.find(key)
		if pos > 0:
			arch = line[pos+len(key):].split(")")[0]
			archs.append(arch)
	universal = "x86_64" in archs, "arm64" in archs
	if not universal:
		print(path, "has archs", archs)
	return universal

def makeLink(target, hasMajorVer):
	parts = os.path.basename(target).split(".")
	target = "./" + os.path.basename(target)
	if hasMajorVer:
		link = os.path.join(frameWorks, parts[0] + "." + parts[1] + ".dylib")
	else:
		link = os.path.join(frameWorks, parts[0] + ".dylib")
	print("link", link, "exists:", os.path.exists(link))
	if os.path.islink(link):
		os.unlink(link)
	os.symlink(target, link)
	

files = os.listdir(frameWorks)
for file in files:
	full = os.path.join(frameWorks, file)
	if os.path.islink(full):
		continue

	# first check the lib is universal
	uni = isUniversal(full)
	if not uni:
		print("error: the library", file, "is not universal")
		sys.exit(1)
		
	# these don't need linka...
	if file.find("libiconv") >= 0 or file.find("libntml") >= 0:
		continue
		
	# then create some symlinks
	isLibZ = file.find("libz") >= 0
	makeLink(full, isLibZ)

sys.exit(0)
