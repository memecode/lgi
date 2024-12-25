#! /usr/bin/env python3
import os
import sys
import subprocess
import shutil
import platform
import stat

arch = []
gen = ["Ninja"] # the default is ninja, except for Windows, which uses Visual Studio
configs = ["Debug", "Release"]
singleConfig = True
universalCheck = []
universalArchs = []
if platform.system() == "Windows":
    subfolders = ["build-x64"]
    gen = ["Visual Studio 16 2019"]
    arch = ["-A", "x64"]
    singleConfig = False
elif platform.system() == "Darwin":    
    subfolders = ["build"]
    universalCheck.append("lib/libiconv.dylib")
    # universalCheck.append("lib/libjpeg.62.4.0.dylib")
    universalCheck.append("lib/libpng16$tag.dylib")
    universalCheck.append("lib/libz.dylib")
    universalCheck.append("lib/liblunasvg.a")
    universalArchs.append('x86_64')
    universalArchs.append('arm64')
elif platform.system() == "Linux":    
    subfolders = ["build-x64"]
elif platform.system() == "Haiku":
    subfolders = ["build-x64"]
else:
    print("Unsupported os:", os.name, platform.system())
    sys.exit(-1)

first = True
clean = len(sys.argv) > 1 and sys.argv[1].lower() == 'clean'
curFolder = os.path.abspath(os.path.join(os.path.realpath(__file__), ".."))
depsFolder = os.path.abspath(os.path.join(curFolder, "..", "..", "deps"))
if not os.path.exists(depsFolder):
    os.mkdir(depsFolder)
print("depsFolder:", depsFolder)
print("curFolder:", curFolder)

def remove_readonly(func, path, excinfo):
    os.chmod(path, stat.S_IWRITE)
    func(path)

if clean:
    shutil.rmtree(depsFolder, onerror=remove_readonly)
    sys.exit(0)

for n in range(len(subfolders)):
    for config in configs:
        path = os.path.abspath(os.path.join(depsFolder, subfolders[n]))
        if singleConfig:
            path = path + "-" + config.lower()

        if config.lower() == "debug":
            tag = "d"
        else:
            tag = ""
        
        if (first or singleConfig or clean) and os.path.exists(path):
            shutil.rmtree(path, onerror=remove_readonly)
        if clean:
            continue

        if not os.path.exists(path):
            os.mkdir(path)

        if first or singleConfig:
            first = False
            print("Configuring:", path)
            args = ["cmake", "-G", gen[n]] + arch
            if singleConfig:
                args += ["-DCMAKE_BUILD_TYPE="+config]
            args += ["-DCMAKE_INSTALL_PREFIX="+path]
            args += ["-DBUILD_SHARED_LIBS=OFF"]
            args += ["-DCMAKE_INSTALL_MANDIR="+path]
            args += ["-DCMAKE_INSTALL_DOCDIR="+path]
            args += [curFolder]

            print("args:", " ".join(args))
            p = subprocess.run(args, cwd=path) # stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
            if p.returncode:
                print("Error: cmake failed.")
                sys.exit(-1)

        print(config, "Build:", path)
        args = ["cmake", "--build", ".", "--target", "install"]
        if not singleConfig:
            args += ["--config", config]
        if 1:
            # Debug: don't hide output
            print("args:", " ".join(args))
            p = subprocess.run(args, cwd=path)
        else:
            p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=path)
        if p.returncode:
            try:
                print(p.stdout.decode())
            except:
                pass
            print("Error: build failed.")
            sys.exit(-1)
        else:
            for file in universalCheck:
                fileName = file.replace("$tag", tag);
                check = os.path.join(path, fileName)
                p = subprocess.run(["file", check], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                output = p.stdout.decode()
                found = []
                for p in output.split("\n"):
                    parts = p.split("(for architecture")
                    if len(parts) > 1:
                        a = parts[1].strip().split(")")[0]
                        if a in universalArchs:
                            found.append(a)
                if len(found) == len(universalArchs):
                    print("Universal check:", check, "ok")
                else:
                    print("Universal check:", check, "ERROR: missing architectures, found:", found)
                    print("output:", output)
                    sys.exit(-1)

