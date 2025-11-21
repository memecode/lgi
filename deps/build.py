#! /usr/bin/env python3
import os
import sys
import subprocess
import shutil
import platform
import stat
import multiprocessing

cores = multiprocessing.cpu_count()
jobs = 2 if cores <= 4 else cores - 1
print("usage: build.py [clean] [installPaths]")
print("cores:", cores, "jobs:", jobs);

first = True
clean = False
installPaths = False
for arg in sys.argv:
    if arg == "--help":
        sys.exit(0)
    if arg.lower() == 'clean':
        clean = True
    elif arg.lower() == 'installpaths':
        installPaths = True

def checkPackage(pkg):
    p = subprocess.run(["dpkg", "-l", pkg], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    for ln in p.stdout.decode().split("\n"):
        p = ln.split()
        if len(p) > 2 and p[1].find(pkg) == 0:
            status = p[0]
            if status == "un":
                return False
            elif status == "ii":
                return True
    return False

def hasPackage(pkg):
    p = subprocess.run(["apt-cache", "search", pkg], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if not p.stdout:
        return False
    for ln in p.stdout.decode().split("\n"):
        if ln.find(pkg):
            return True
    return False

arch = []
gen = ["Ninja"] # the default is ninja, except for Windows, which uses Visual Studio
configs = ["Debug", "Release"]
singleConfig = True
universalCheck = []
universalArchs = []
extraCmakeArgs = []
if platform.system() == "Windows":
    subfolders = ["build-x64"]
    gen = ["Visual Studio 16 2019"]
    arch = ["-A", "x64"]
    singleConfig = False
elif platform.system() == "Darwin":
    subfolders = ["build"]
    universalCheck.append("lib/libiconv.dylib")
    universalCheck.append("lib/libjpeg.62.4.0.dylib")
    universalCheck.append("lib/libpng16$tag.dylib")
    universalCheck.append("lib/libz.dylib")
    universalCheck.append("lib/liblunasvg.a")
    universalArchs.append('x86_64')
    universalArchs.append('arm64')
elif platform.system() == "Linux":
    subfolders = ["build-x64"]
    packages = ["build-essential",
                "mercurial",
                "libmagic-dev", 
                "libgtk3.0-dev",
                "libgtk-3-dev",
                "libgstreamer1.0-dev",
                "libayatana-appindicator3-dev",
                "libssh-dev",
                "cmake",
                "ninja-build",
                "git" ]
    needs = []
    print("Checking required packages:")
    for pkg in packages:
        c = checkPackage(pkg)
        # print("  check", pkg, c)
        if not c:
            h = hasPackage(pkg)
            # print("    has", pkg, h)
            if h:
                needs.append(pkg)
    if len(needs) > 0:
        print("    sudo apt install", " ".join(needs))
        sys.exit(-1)
elif platform.system() == "Haiku":
    subfolders = ["build-x64"]
    # extraCmakeArgs += ["-DCMAKE_POSITION_INDEPENDENT_CODE=ON"]
else:
    print("Unsupported os:", os.name, platform.system())
    sys.exit(-1)

curFolder = os.path.abspath(os.path.join(os.path.realpath(__file__), ".."))
trunkFolder = os.path.abspath(os.path.join(curFolder, ".."))
depsFolder = os.path.abspath(os.path.join(trunkFolder, "..", "deps"))
if not os.path.exists(depsFolder):
    os.mkdir(depsFolder)

def remove_readonly(func, path, excinfo):
    os.chmod(path, stat.S_IWRITE)
    func(path)

if clean:
    shutil.rmtree(depsFolder, onerror=remove_readonly)
    print("deleted:", depsFolder)
    sys.exit(0)

if installPaths:
    if platform.system() == "Linux":
        codeFolder = os.path.abspath(os.path.join(trunkFolder, "..", ".."))
        scribeLibs = os.path.join(codeFolder, "scribe", "libs")
        
        paths = [
            os.path.join(depsFolder, "build-x64-debug/lib"),
            os.path.join(depsFolder, "build-x64-release/lib"),
            os.path.join(trunkFolder, "Debug"),
            os.path.join(trunkFolder, "Release"),
        ]

        if os.path.isdir(scribeLibs):
            paths.append(os.path.join(scribeLibs, "build-x64-debug/lib"))
            paths.append(os.path.join(scribeLibs, "build-x64-release/lib"))

        cfgFile = "/etc/ld.so.conf.d/lgi.conf"
        try:
            conf = open(cfgFile, "w")
            for p in paths:
                conf.write(p + "\n")
            conf.close()
            p = subprocess.run(["ldconfig"])
            print("..paths added to:", cfgFile)
            for p in paths:
                print("   ", p)
        except:
            print("Add paths to:", cfgFile)
            for p in paths:
                print("   ", p)
            print("Then: sudo ldconfig")
    else:
        print("Add impl for", platform.system(), "here")
    sys.exit(0)

print("depsFolder:", depsFolder)
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
            if len(extraCmakeArgs) > 0:
                args += extraCmakeArgs
            args += [curFolder]

            print("args:", " ".join(args))
            p = subprocess.run(args, cwd=path) # stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
            if p.returncode:
                print("Error: cmake failed.")
                sys.exit(-1)

        print(config, "Build:", path)
        args = ["cmake", "--build", ".", "--target", "install"]
        args += ["-j" + str(jobs)]
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

