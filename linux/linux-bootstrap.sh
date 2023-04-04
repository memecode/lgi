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
if [ ! -f /usr/lib/liblgi-gtk3d.so ]; then
	sudo ln -s "$WD/Debug/liblgi-gtk3d.so" /usr/lib/liblgi-gtk3d.so
fi
if [ ! -f /usr/lib/liblgi-gtk3.so ]; then
	sudo ln -s "$WD/Release/liblgi-gtk3.so" /usr/lib/liblgi-gtk3.so
fi
