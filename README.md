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

Note: in the following document the convention is that anything starting 
with `lgi/...` means the `~/code/lgi/...` in the above example. It will
generally have 2 subfolders `trunk` and `deps` once built.

### Building the dependencies

Lgi has a built in dependency builder in `deps`:

     ~/code$ cd lgi/trunk/deps
     ~/code/lgi/trunk/deps$ ./build.py
     
>#### Linux specific:
>The `build.py` will mention if you need to install various packages first.
>
>Then you can ask it to add various paths to ldconfig with:
>```
>~/code/lgi/trunk/deps$ sudo ./build.py installpaths
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
>Load  and build [win/Lgi_vs2019.sln](https://github.com/memecode/lgi/blob/main/win/Lgi_vs2019.sln)
>#### vs2022
>Load  and build [win/Lgi_vs2022.sln](https://github.com/memecode/lgi/blob/main/win/Lgi_vs2022.sln)
>
>#### Add these library folders to your PATH:
> ```
> lgi\deps\build-x64\bin
> lgi\trunk\lib
>```

### Building using XCode:
>Open [lgi/trunk/src/mac/cocoa/LgiCocoa.xcodeproj](https://github.com/memecode/lgi/tree/main/src/mac/cocoa/LgiCocoa.xcodeproj) in XCode and then build.

## Creating an application

One way of starting an application is to use `LgiIde`:

>#### Windows
>
>Build and run [lgi/trunk/ide/win/LgiIde.sln](https://github.com/memecode/lgi/blob/main/ide/win/LgiIde.sln)
>
>Use the menu Project 🡒 New From Template
>
>- Give it a name
>- Select the template to use `windows/basic_gui_2019`
>- Select an output path and click `Create`
>- Now open the new `.sln` solution in the output folder and build from there.
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

### Build settings:
#### Windows
Add the project [win\Lgi_vs2019.vxproj](https://github.com/memecode/lgi/blob/main/win/Lgi_vs2019.vcxproj) to your workspace.
Then in your new project you'll need to set these settings:

* C/C++ tab
	- C++ Language
		- RTTI on.
	- Code Generation
		- [Debug] Run time library: Debug Multithreaded DLL
		- [Release] Run time library: Multithreaded DLL
	- Preprocessor
		- Define: LGI_STATIC (if your using LgiStatic version)
		- Include path: `lgi/trunk/include`
* Link tab
	- Object/library modules:
		- Add `imm32.lib` (if you use LTextView3.cpp)

#### Linux/Cygwin
* Add to your library string:
	- [Debug] `-Llgi/trunk/Debug -llgid`
	- [Release] `-Llgi/trunk/Release -llgi`
* Add to your compile flags:
	- `-ilgi/trunk/include`

#### Mac
* Include the `src/cocoa/LgiCocoa.xcodeproj` into your app
	- Select your top level project in the tree
	- Select your target
	- In `Build Settings 🡒 Search Paths 🡒 Header Search Paths` add `lgi/trunk/include`
	- In `Build Settings 🡒 Custom Compiler Flags 🡒 Other C++ Flags` add `-DMAC -DPOSIX -DLGI_COCOA`
		- And for the `Debug` sub-setting also add `-D_DEBUG`
	- In `Build Phases 🡒 Target Dependencies` add `LgiCocoa`
	- In `Build Phases 🡒 Link Binary With Libraries` add `LgiCocoa`

## Writing applications

There is a fairly complete list of features on the [Lgi Homepage](https://www.memecode.com/lgi/#features) 
with links into the source code itself.

The general pattern for bootstrapping an application is:

- Add [LgiMain.cpp](https://github.com/memecode/lgi/blob/main/src/common/Lgi/LgiMain.cpp) and then in a new source file
- implement [LgiMain](https://github.com/memecode/lgi/blob/main/src/common/Lgi/LgiMain.cpp#L36) that
- instantiates an [LApp](https://github.com/memecode/lgi/blob/main/include/lgi/common/App.h#L105) object and
- then creates an instance of a new application class that overrides 
	[LWindow](https://github.com/memecode/lgi/blob/main/include/lgi/common/Window.h#L31). Assign that to
	[LApp::AppWnd](https://github.com/memecode/lgi/blob/main/include/lgi/common/App.h#L204).
- finally start the message pump by calling [LApp::Run](https://github.com/memecode/lgi/blob/main/include/lgi/common/App.h#L245)

There is a fairly bare bones example of this in
[lgi/trunk/templates/windows/basic_gui_vs2019](https://github.com/memecode/lgi/tree/main/templates/windows/basic_gui_vs2019).
It's not windows specific though, and the code in
[Main.cpp](https://github.com/memecode/lgi/blob/main/templates/windows/basic_gui_vs2019/Main.cpp)
will run on all supported platforms.

If you want to look at some more complete examples, see:

- [lgi/trunk/ide](https://github.com/memecode/lgi/tree/main/ide)
- [lgi/trunk/lvc](https://github.com/memecode/lgi/tree/main/lvc)
- [lgi/trunk/resourceEditor](https://github.com/memecode/lgi/tree/main/resourceEditor)
