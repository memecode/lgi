#!/usr/bin/env bash

sudo apt-get -y install python3 libmagic-dev libgtk-3-dev libgstreamer1.0-dev cmake libssl-dev libjpeg-dev libpng-dev libssh-dev
ln -s Makefile.linux makefile

(cd Ide && ln -s Makefile.linux makefile)
(cd Ide && make -j 4)

(cd Lvc && ln -s Makefile.linux makefile)
(cd Lvc && make -j 4)

(cd ResourceEditor && ln -s Makefile.linux makefile)
(cd ResourceEditor && make -j 4)

WD=$(pwd)
echo "Add these paths to your .profile's LD_LIBRARY_PATH:"
echo "$WD/Debug"
echo "$WD/Release"
