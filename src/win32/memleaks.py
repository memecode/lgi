import os
import sys

total = 0
count = 0
d = dict()
lines = open("memleaks.txt", "r").read().replace("\r","").split("\n")
for ln in lines:
	if ln.find("block at") >= 0:
		if ln[0] != '{':
			# app mem
			src = ln.split(" : ")[0].strip()
			if src in d:
				d[src] = d[src] + 1
			else:
				d[src] = 1
		else:
			# non app mem
			sz = ln.split(",")[-1].split()[0]
			total = total + int(sz)
			count = int(count) + 1
		

for k,v in d.items():
	print(k,v)
print("other:", total, "in", count, "objects")