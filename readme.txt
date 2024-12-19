	    Lightweight GUI Library
	    -----------------------

The primary aim of LGI is to abstract away the differences between
operating system's and provide a consistent API that applications
can target.

As a secondary goal the library should strive to be compact for easy
distribution with an application without bloating out the download.
Also the API is designed to be as simple as possible for the programmer
to use, without sacrificing functionality on the various platforms
supported.

The library is not intended to be shared between applications
as there is too much change in the API and object size to warrant a
shared library approach. In the future this may change if compatibility
can be improved.

Compiling LGI
-------------

    Open 'include/common/Lgi.h' and check through any compile time
    options there. You may want to switch things on or off to get it to
    compile.

    Win32:
    	Load  win/Lgi_vs2019.sln into Visual Studio 2019 and build it.
    
    Linux:
    	ln -s linux/Makefile.linux Makefile
		make -j $THREADS
    
    Mac:
    	Open src/mac/cocoa/LgiCocoa.xcodeproj in XCode and then build.
	
	Add build folders so the OS can find the shared libraries:

		* For Windows add these to your path:
			- ./lib
		* On Linux, create symlinks in /usr/lib to the files:
			- ./Debug/liblgid.so
			- ./Release/liblgi.so

Building Your App
-----------------
    Win32: Add the project [lgi]/Lgi_vc9.vxproj to your workspace. Then in your 
    new project you'll need to set these settings:
	    * C/C++ tab
			- C++ Language
			    - RTTI on.
			- Code Generation
			    - [Debug] Run time library: Debug Multithreaded DLL
			    - [Release] Run time library: Multithreaded DLL
			- Preprocessor
			    - Define: LGI_STATIC (if your using LgiStatic version)
			    - Include path: include/common
			    - Include path: include/win32
		* Link tab
			- Object/library modules:
				- Add 'imm32.lib' (if you use GTextView3.cpp)

    Linux/Cygwin:
    	* Add to your library string:
    		- [Debug] '-L[Lgi]/Debug -llgid'
    		- [Release] '-L[Lgi]/Release -llgi'
    	* Add to your compile flags:
    		- '-i./include/common -i./include/linux/X'

	Mac:
		* Include the lgi.framework into your app
		* Add the these folders to your include path:
			- include/common
			- include/mac

Usage
-----
    In most cases just:

	#include "lgi/common/Lgi.h"

    But you can also include "Gdc2.h" for just graphics support without all
    the GUI stuff.

    The naming conventions for container methods are:
	- "Delete(...)" removes from the container and frees the object.
	- "Remove(...)" just removes from the container.

Documentation
-------------
    
    See: docs/html/index.html.

	Which you may have to generate with doxygen if you are building from the
	repository.

