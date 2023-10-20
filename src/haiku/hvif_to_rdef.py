#!/usr/bin/env python3
import os
import sys

# Convert hvif to rdef

# get arguments
if len(sys.argv) != 4:
	print("Usage: hvif_to_rdef.py <hvifFile> <rdefFile> <appSig>")
	sys.exit(-1)
	
hvif = sys.argv[1]
rdef = sys.argv[2]
appSig = sys.argv[3]

hvifSize = os.stat(hvif).st_size

try:
	input = open(hvif, "rb")
	out = open(rdef, "w")
except:
	print("Error openning files.")
	sys.exit(-2)

# app sig:
out.write("resource app_signature \"application/x-vnd." + appSig + "\";\n\n")

# icon:
out.write("resource vector_icon {\n");

# loop through data here...
i = 0
# print("hvifSize:", hvifSize)
while i < hvifSize:
	remaining = hvifSize - i
	block = min(remaining, 32)
	
	out.write("    $\"");
	for n in range(block):
		out.write("%2.2X" % (input.read(1)[0]))
		
	out.write("\"\n")
	i = i + block

out.write("};\n");
out.close()
# print("rdefSize:", os.stat(rdef).st_size)
sys.exit(0)
