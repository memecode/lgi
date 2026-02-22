#! /usr/bin/env python3
import os
import sys
import subprocess
import shutil

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
    return None

def getSlnVsVer(sln):
    versions = {(16, 2019),
                (17, 2022),
                (18, 2026)}

    lines = open(sln, "r").read().replace("\r", "").split("\n")
    for ln in lines:
        # print("ln:", ln)
        if ln[0] == "#":
            if ln.find("2019") > 0:
                return 2019
            elif ln.find("2022") > 0:
                return 2022
        elif ln.find("VisualStudioVersion") >= 0:
            p = ln.split("=", 1)
            fullVer = p[-1].strip().split(".")
            for ver in versions:
                # print("ver:", ver, ver[0], fullVer[0], )
                if int(ver[0]) == int(fullVer[0]):
                    return ver[1]
    return None

# check if the sln name has the correct postfix for the version of sln it is
def nameOk(path):
    ext = path.rsplit(".", 1)[-1]
    if ext == "sln":
        sln = path
    elif ext == "vcxproj":
        sln = path.rsplit(".", 1)[0] + ".sln"
    else:
        print("nameOk err: unexpected ext:", ext)
        sys.exit(-1)
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

def newName(sln):
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
    if len(p) == 1:
        p.append(correctPostfix)
    else:
        p[1] = correctPostfix
    return os.path.join(os.path.dirname(sln), "_".join(p))


def vcsCommand(vcs, cmd, oldPath, newPath):

    if vcs == "git" and cmd == "cp":
        # no git copy command, so....
        shutil.copyfile(oldPath, newPath)
        args = [vcs, "add", newPath]
    else:
        args = [vcs, cmd, oldPath, newPath]

    print("vcsCommand:", args)
    p = subprocess.run(args, stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
    if p.returncode:
        print("vcsCommand error:", p.stdout.decode())
    return p.returncode == 0

# returns the new file name or None on error
def renameSln(sln):
    slnVer = getSlnVsVer(sln)
    if slnVer is None:
        print("renameSln: no sln ver:", sln)
        return None
    vcs = getVcsType(sln)
    if vcs is None:
        print("renameSln: no vcs:", sln)
        return None    

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

        vcsCommand(vcs, "mv", oldSln,     newSln)
        vcsCommand(vcs, "mv", oldProj,    newProj)
        vcsCommand(vcs, "mv", oldFilters, newFilters)
        if os.path.exists(oldUser):
            os.rename(oldUser, newUser)

    return newSln

def checkProjLink(sln, targetVsVer = None):
    changed = False
    if os.path.exists(sln):
        print("sln open:", sln)
        f = open(sln, "r")
        txt = f.read()
        lines = txt.replace("\r", "").split("\n")
    else:
        return
    for idx in range(len(lines)):
        ln = lines[idx]
        # print("ln:", ln)
        if ln.find("Project(") == 0:
            vars = ln.split("=")
            parts = vars[1].split(",")
            proj = parts[0].strip().strip("\"")
            path = parts[1].strip().strip("\"")
            fullPath = os.path.abspath(path)
            print(vars[0], proj, "=", path)
            if proj == "Lgi":
                if path.find("_vs2019"):
                    changed = True
                    if targetVsVer is None:
                        vars[1] = vars[1].replace("_vs2019", "_vs19")
                    else:
                        vars[1] = vars[1].replace("_vs2019", "_vs22")
                    lines[idx] = "=".join(vars)
                    print("newline:", lines[idx])
                elif path.find("_vs19") and \
                    not targetVsVer is None and \
                    targetVsVer == 2022:
                    changed = True
                    vars[1] = vars[1].replace("_vs19", "_vs22")
                    lines[idx] = "=".join(vars)
                    print("newline:", lines[idx])
            elif not os.path.exists(fullPath) or not nameOk(path):
                base = path.rsplit(".", 1)[0]
                newSln = None
                if os.path.exists(base + "_vs19.vcxproj"):
                    newPath = base + "_vs19.vcxproj"
                    changed = True
                    vars[1] = vars[1].replace(path, newPath)
                    lines[idx] = "=".join(vars)
                    print("######## sln name change:", path, newPath)
                elif os.path.exists(base + "_vs22.vcxproj"):
                    newPath = base + "_vs22.vcxproj"
                    changed = True
                    vars[1] = vars[1].replace(path, newPath)
                    lines[idx] = "=".join(vars)
                    print("######## sln name change:", path, newPath)
                else:
                    print("######## need to fix the path:", fullPath, base)
    if changed:
        txt = "\n".join(lines)
        print("\nnewtxt:")
        for ln in lines:
            print("    ln:", ln)
        if 1: # write changes
            open(sln, "w").write(txt)
    

sln = os.path.abspath(sys.argv[1])
vcs = getVcsType(sln)
print("vcs:", vcs)

slnVer = getSlnVsVer(sln)
print("slnVer:", slnVer)

if nameOk(sln):
    print("sln name is ok")
else:
    sln = renameSln(sln)
    if sln is None:
        sys.exit(-1)
    

print("sln:", sln)
checkProjLink(sln)

if slnVer == 2019:
    # check if there is a 2022 version of the solution
    oldProj    = sln.rsplit(".", 1)[0] + ".vcxproj"
    oldFilters = sln.rsplit(".", 1)[0] + ".vcxproj.filters"
    newSln     = sln.rsplit("_", 1)[0] + "_vs22.sln"
    if not os.path.exists(newSln):
        newProj = oldProj.replace("19", "22")
        newFilters = oldFilters.replace("19", "22")
        print("2022 sln:", newSln)
        print("2022 prj:", newProj)
        print("2022 fil:", newFilters)
        
        vcsCommand(vcs, "cp", sln, newSln)
        vcsCommand(vcs, "cp", oldProj, newProj)
        vcsCommand(vcs, "cp", oldFilters, newFilters)
        
        checkProjLink(newSln, 2022)
