#! /usr/bin/env python3
import os
import sys
import subprocess

if len(sys.argv) < 2:
    print("usage:", sys.argv[0], " <slnPath>")
    sys.exit(-1)


def getVcsType(path):
    parts = os.path.abspath(path).split(os.path.sep)
    print("parts:", parts)
    for i in range(len(parts)-1, 1, -1):
        base = os.path.sep.join(parts[0:i])
        if os.path.exists(os.path.join(base, ".hg")):
            return "hg"
        elif os.path.exists(os.path.join(base, ".git")):
            return "git"
        print("i:", i, base)

def getSlnVsVer(sln):
    versions = {(16, 2019),
                (17, 2022),
                (18, 2026)}

    lines = open(sln, "r").read().replace("\r", "").split("\n")
    for ln in lines:
        # print("ln:", ln)
        if ln.find("VisualStudioVersion") >= 0:
            p = ln.split("=", 1)
            fullVer = p[-1].strip().split(".")
            for ver in versions:
                # print("ver:", ver, ver[0], fullVer[0], )
                if int(ver[0]) == int(fullVer[0]):
                    return ver[1]
    return None

# check if the sln name has the correct postfix for the version of sln it is
def nameOk(sln):
    slnVer = getSlnVsVer(sln)
    if slnVer is None:    
        print("nameOk: no sln ver:", sln)
        return False    
    leaf = os.path.basename(sln)
    parts = leaf.split(".")
    p = parts[0].split("_")
    if len(p) != 2:
        print("nameOk: leaf has wrong number of parts between underscores:", p)
        return False
    correctPostfix = "vs" + str(slnVer)[2:]
    if p[1] != correctPostfix:
        print("nameOk: postfix not:", correctPostfix)
        return False    
    return True

def vcsRename(vcs, oldPath, newPath):
    args = [vcs, "mv", oldPath, newPath]
    print("vcsRename:", args)
    p = subprocess.run(args, stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
    if p.returncode:
        print("vcsRename error:", p.stdout.decode())
    return p.returncode == 0

def renameSln(sln):
    slnVer = getSlnVsVer(sln)
    if slnVer is None:
        print("renameSln: no sln ver:", sln)
        return False    
    vcs = getVcsType(sln)
    if vcs is None:
        print("renameSln: no vcs:", sln)
        return False    

    leaf = os.path.basename(sln)
    parts = leaf.split(".")
    p = parts[0].split("_")
    correctPostfix = "vs" + str(slnVer)[2:]
    if len(p) == 1:
        p.append(correctPostfix)
    else:
        p[1] = correctPostfix
    newLeaf = "_".join(p)
    print("newLeaf:   ", newLeaf)

    folder = os.path.dirname(sln)
    
    oldBase = os.path.join(folder, parts[0])
    oldSln = oldBase + "." + parts[1]
    oldProj = oldBase + ".vcxproj"
    oldFilters = oldBase + ".vcxproj.filters"
    oldUser = oldBase + ".vcxproj.user"

    print("oldSln:    ", oldSln, os.path.exists(oldSln))
    print("oldProj:   ", oldProj, os.path.exists(oldProj))
    print("oldFilters:", oldFilters, os.path.exists(oldFilters))
    print("oldUser:   ", oldUser, os.path.exists(oldUser))

    newBase = os.path.join(folder, newLeaf)
    newSln = newBase + "." + parts[1]
    newProj = newBase + ".vcxproj"
    newFilters = newBase + ".vcxproj.filters"
    newUser = newBase + ".vcxproj.user"
    
    print("newSln:    ", newSln)
    print("newProj:   ", newProj)
    print("newFilters:", newFilters)
    print("newUser:   ", newUser)
    
    if  os.path.exists(oldSln) and \
        os.path.exists(oldProj) and \
        os.path.exists(oldFilters):

        vcsRename(vcs, oldSln,     newSln)
        vcsRename(vcs, oldProj,    newProj)
        vcsRename(vcs, oldFilters, newFilters)
        if os.path.exists(oldUser):
            os.rename(oldUser, newUser)

        return True
    
    return False

sln = os.path.abspath(sys.argv[1])
vcs = getVcsType(sln)
print("vcs:", vcs)

slnVer = getSlnVsVer(sln)
print("slnVer:", slnVer)

if not nameOk(sln):
    renameSln(sln)

