#!/usr/bin/env python3
import os
import sys
import subprocess

if len(sys.argv) < 2:
	print("usage: setDesktop.py [\"wayland\"|\"x11\"]")
	sys.exit(-1)

isWayland = sys.argv[1].lower() == "wayland"
isX11 = sys.argv[1].lower() == "x11"
print("isWayland:", isWayland, "isX11:", isX11)
if not isWayland and not isX11:
	print("error: invalid param")
	sys.exit(-1)

sConfigFile = "/etc/gdm3/custom.conf"

try:
	f = open(sConfigFile, "r")
	txt = f.read().strip()
	f.close()
	
	out = []
	for ln in txt.split("\n"):
		# print("ln:", ln)
		if ln.find("WaylandEnable") >= 0:
			if isWayland:
				out.append("WaylandEnable=true")
			elif isX11:
				out.append("WaylandEnable=false")
			else:
				out.append(ln)
		else:
			out.append(ln)
			
	print("out:", "\n".join(out))
	
	f = open(sConfigFile, "w")
	f.write("\n".join(out) + "\n")
	f.close()
	
	# log out and start in the new desktop...
	subprocess.run(["systemctl", "restart", "gdm3"]);
		
except:
	sys.exit(-1);


