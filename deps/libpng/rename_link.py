import os
import sys
import platform

if platform.system() != "Linux":
    print("warning: this script is not needed on", platform.system())
    sys.exit(2)

print("Renaming libpng link files:", sys.argv[0])
depsDir = sys.argv[1]
configs = ["debug", "release"]
hasErr = False
for config in configs:
    configDir = os.path.join(depsDir, "build-x64-" + config, "lib")
    srcLink = os.path.join(configDir, "libpng.so")
    dstLink = os.path.join(configDir, "libpng-lgi.so")
    if not os.path.exists(srcLink) and os.path.exists(dstLink):
        print("Success: dst link exists: " + dstLink)
    elif os.path.exists(srcLink):
        os.rename(srcLink, dstLink)
        print("Success: renamed: " + srcLink + " -> " + dstLink)
    else:
        print("Error: Could not find link file: " + srcLink)
        hasErr = True

if hasErr:
    sys.exit(1)