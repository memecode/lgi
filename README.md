# Lightweight GUI Library

The primary aim of LGI is to abstract away the differences between
operating system's and provide a consistent API that applications
can target.

As a secondary goal the library should strive to be compact for easy
distribution with an application without bloating out the download.
Also the API is designed to be as simple as possible for the programmer
to use, without sacrificing functionality on the various platforms
supported.

The library is not intended to be shared between applications
as there is too much change in the API to warrant a true
shared library approach. Generally a private build of Lgi is installed
along side the binary of the application.

## Setup

### Clone the code:

Assuming you have `git` installed:

    ~$ mkdir code
    ~/code$ git clone https://github.com/memecode/lgi.git lgi/trunk

### Building the dependencies

Lgi has a built in dependency builder in `deps`:

     ~/code$ cd lgi/trunk/deps
     ~/code/lgi/trunk/deps$ ./build.py
     
>#### Linux specific:
>The `build.py` will mention if you need to install various packages first.
>
>Then you can ask it to add various paths to ldconfig with:
>```
~/code/lgi/trunk/deps$ sudo ./build.py installpaths
>```
#### Cross platform

Note that there are some compile time options in `include/common/Lgi.h`.
You may want to switch things on or off to get it to
compile.

### Building using make:
>(For Haiku, substitute `haiku` for `linux` here)
>```
>~/code/lgi/trunk$ ln -s linux/Makefile.linux makefile
>~/code/lgi/trunk$ make -j ##
>```

### Building using cmake:
>Assuming you have `ninja` installed:
>```
>~/code/lgi/trunk$ mkdir build
>~/code/lgi/trunk$ cd build
>~/code/lgi/trunk/build$ cmake -GNinja ..
>~/code/lgi/trunk/build$ ninja
>```

### Building using Visual Studio:
>#### vs2019
>Load  and build `win/Lgi_vs2019.sln`
>#### vs2022
>Load  and build `win/Lgi_vs2022.sln`
>
>#### Add binary folders to your PATH:
> ```
> ~/code/lgi/deps/build-debug/lib
> ~/code/lgi/deps/build-release/lib
> ~/code/lgi/trunk/win/debug
> ~/code/lgi/trunk/win/release
>```

### Building using XCode:
>Open `src/mac/cocoa/LgiCocoa.xcodeproj` in XCode and then build.

## Creating an application

One way of starting an application is to use `LgiIde`:

>#### Windows
>
>Build and run `lgi/trunk/ide/win/LgiIde.sln`
>
>Use the menu Project 🡒 New From Template
>
>- Give it a name
>- Select the template to use `windows/basic_gui_2019`
>- Select an output path and click `Create`
>- Now only the new `.sln` file in the output folder and build from there.
>
>#### Linux
>```
>~$ cd code/lgi/trunk/ide
>~/code/lgi/trunk/ide$ ln -s linux/Makefile.linux makefile
>~/code/lgi/trunk/ide$ make -j##
>~/code/lgi/trunk/ide$ ./lgiide
>```
>
>- Then use the menu: Project 🡒 New
>- Save the project somewhere as `$ProjectName`
>- Right click `$ProjectName` and select `Insert Dependency`, and navigate to `lgi/trunk/Lgi.xml`
>- Right click `$ProjectName` and select `Settings`, click `Set Defaults`
>- Right click `$ProjectName/Source` and add `lgi/trunk/src/common/Lgi/LgiMain.cpp`
>- Start adding your own source, implementing `LgiMain` somewhere...

### Win32:
Add the project `win\Lgi_vs2019.vxproj` to your workspace. Then in your 
new project you'll need to set these settings:

* C/C++ tab
	- C++ Language
		- RTTI on.
	- Code Generation
		- [Debug] Run time library: Debug Multithreaded DLL
		- [Release] Run time library: Multithreaded DLL
	- Preprocessor
		- Define: LGI_STATIC (if your using LgiStatic version)
		- Include path: `~/code/lgi/trunk/include`
* Link tab
	- Object/library modules:
		- Add `imm32.lib` (if you use LTextView3.cpp)

### Linux/Cygwin:
* Add to your library string:
	- [Debug] `-L~/code/lgi/trunk/Debug -llgid`
	- [Release] `-L~/code/lgi/trunk/Release -llgi`
* Add to your compile flags:
	- `-i~/code/lgi/trunk/include`

### Mac:
* Include the `lgi.framework` into your app
* Add the these folders to your include path:
	- `~/code/lgi/trunk/include`

## Usage

In most cases just:

    #include "lgi/common/Lgi.h"

But you can also include `Gdc2.h` for just graphics support without all
the GUI stuff.

The naming conventions for container methods are:

- "Delete(...)" removes from the container and frees the object.
- "Remove(...)" just removes from the container.

## Documentation
    
See: docs/html/index.html.

Which you may have to generate with doxygen if you are building from the
repository.

