
# ![Joulescope](https://download.joulescope.com/press/joulescope_logo-PNG-Transparent-Exact-Small.png "Joulescope Logo")

Welcome to the Joulescope™ JS110 statistics library and application. 
[Joulescope](https://www.joulescope.com) is an affordable, precision DC energy 
analyzer that enables you to build better products. Most Joulescope 
programs use the python [pyjoulescope](https://github.com/jetperch/pyjoulescope)
driver. Although it works great, the python implementation
can be more difficult to include and distribute in large systems,
such as test automation frameworks.

This repository contains a self-contained library and application written 
in C, currently for Windows only.  The goal of the library is to provide
a very simple API to get the 2 Hz statistics from all Joulescopes 
connected to a host computer.  The library:

* Scans for all connected Joulescopes and rescans on any device changes.
* Opens all connected Joulescopes.
* Fetches statistics information from the connected Joulescopes.
* Calls your callback with each statistics update.

See [main.c](source/main.c) for the example application.


## Why?

The standard 
[pyjoulescope](https://github.com/jetperch/pyjoulescope),
[pyjoulescope_ui](https://github.com/jetperch/pyjoulescope_ui), and
[Joulescope UI](https://www.joulescope.com/download) are great. 
We recommend trying those first. 

However, they do have a limitations for some scenarios.
The Joulescope UI always streams data from the JS110 at the full 2,000,000
samples per second. That's a lot of data, which limits the number of 
Joulescopes that you can connect to a single host. For long-term testing 
and qualification, you need Joulescope's fast autoranging and accurate 
statistics, but not the full-rate data. This library and application 
allows much more efficient data collection since the statistics are 
computed on the instrument. This library can support up to 127 Joulescopes 
attached to single host computer. It has a simple API consisting of just
two functions and a callback, all written in C, so it is easy to integrate
into nearly any other program.  Finally, it is also relatively simple,
so you can translate it to your own programming language to remove any
C dependency.

Most Joulescope users will run the graphical user interface which is in the 
[pyjoulescope_ui](https://github.com/jetperch/pyjoulescope_ui) package and
available for [download](https://www.joulescope.com/download).


## Requirements

* Windows 10 (Windows 7 will likely work, but is untested).
* Joulescopes upgraded to firmware 1.3.2+.
  You can use Joulescope UI 0.9.2+ to easily perform the upgrade.


## Building

This project currently supports Windows only. 
Install [CMake](https://cmake.org/), the meta-build system.  You will also
need one of:

* [Visual Studio](https://visualstudio.microsoft.com/vs/) (paid)
* [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019) (free)
* [mingw-w64](http://mingw-w64.org/doku.php) (free)

Clone or download this repository to the directory of your choice.

If building under Visual Studio, open a command prompt, and type:

    cd {your_directory}
    mkdir build
    cd build
    cmake ..
    cmake --build . --config Release
    
    
If building under MinGW, open a command prompt, and type:

    cd {your_directory}
    mkdir build
    cd build
    set MINGW=C:\bin\mingw-w64\i686-8.1.0-posix-dwarf-rt_v6-rev0\mingw32
    set PATH=%MINGW%\bin;%PATH%
    cmake.exe -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles" ..
    cmake --build .
    
Note that must change the "set MINGW" line to your actual installation path.


## License

All pyjoulescope code is released under the permissive Apache 2.0 license.
See the [License File](LICENSE.txt) for details.
