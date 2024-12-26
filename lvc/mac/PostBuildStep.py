#!/usr/bin/env python3
import os
import sys
# import subprocess

#log = open("log-file.txt", "w")
#log.write(str(sys.argv))

frameworks = sys.argv[1]
print("frameworks:", frameworks)

def makeLink(target, hasMajorVer):
	global frameworks
	global log

	parts = target.split(".")
	target = "./" + target
	first = parts[0]
	if first[-1] == "d":
		first = first[0:-1]
	if hasMajorVer:
		link = os.path.join(frameworks, first + "." + parts[1] + ".dylib")
	else:
		link = os.path.join(frameworks, first + ".dylib")
	print("link", link, "exists:", os.path.exists(link))
	if os.path.islink(link):
		os.unlink(link)
	os.symlink(target, link)

def findLibAndLink(name, hasMajorVer):
	global frameworks
	global log

	files = os.listdir(frameworks)
	for f in files:
		full = os.path.join(frameworks, f)
		if not os.path.islink(full) and f.find(name) >= 0:
			makeLink(f, hasMajorVer)

findLibAndLink("libz", True)
findLibAndLink("libpng16", False)
