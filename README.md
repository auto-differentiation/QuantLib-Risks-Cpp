# QuantLib - XAD Integration Repository

This repository contains integration headers, examples, and tests required when 
using the [XAD](https://github.com/xcelerit/xad) algorithmic differentiation
library with [QuantLib](https://github.com/lballabio/QuantLib).
It is not usable outside of this context.

## Building

### 1. Repository Clone/Checkout

Clone these three repositories into separate folders:

- https://github.com/xcelerit/qlxad.git
- https://github.com/xcelerit/xad.git
- https://github.com/lballabio/QuantLib.git

This repository follows QuantLib's release cycle, using the same version numbers
as the corresponding QuantLib version.
That is:

- For tagged releases, checkout the same release tag for QuantLib and this repository
- For QuantLib master, check out this repository's main branch

For short, these are the commands to checkout release 1.28:

```shell
git clone https://github.com/xcelerit/xad.git
git clone --branch v1.28 https://github.com/xcelerit/qlxad.git
git clone --branch QuantLib-v1.28 https://github.com/lballabio/QuantLib.git
```

(Or, to work on the latest master/main branch, omit the `--branch xxx` option.)

### 2. Install Boost

A recent version of boost is a requirement for building QuantLib.
If you do not have it already, you need to install it into a system path.
You can do that in one of the following ways, depending on your system:

- Ubuntu or Debian: `sudo apt install libboost-all-dev`
- Fedora or RedHat: `sudo yum install boost-devel`
- MacOS using [Homebrew](https://brew.sh/): `brew install boost`
- MacOS using [Mac Ports](https://www.macports.org/): `sudo port install boost`
- Windows using [Chocolatey](https://chocolatey.org/): 
   
   - For Visual Studio 2022: `choco install boost-msvc-14.3`
   - For Visual Studio 2019: `choco install boost-msvc-14.2`
   - For Visual Studio 2017: `choco install boost-msvc-14.1`
   
- Windows using manual installers: [Boost Binaries on SourceForge](https://sourceforge.net/projects/boost/files/boost-binaries/)

### 3. Install CMake

You will also need a recent version of CMake (minimum version 3.15.0). 
You can also install this with your favourite package manager 
e.g. apt, yum, homebrew, chocolatey as above), or obtain it from 
the [CMake downloads page](https://cmake.org/download/).

Note that Microsoft ships Visual Studio with a suitable version 
command-line only version of CMake since Visual Studio 2019 
(the Visual Studio 2017 CMake version is outdated). 
It is available in the `PATH` from a Visual Studio command prompt
and can alternatively be used directly from the IDE.

### 4. QuantLib CMake Configuration

The build is driven from the QuantLib directory - XAD and qlxad are
inserted using QuantLib's external subdirectories hook. 

Configure the QuantLib CMake build with setting the following parameters:

- `QL_EXTERNAL_SUBDIRECTORIES=/path/to/xad;/path/to/qlxad`
- `QL_EXTRA_LINK_LIBRARIES=qlxad`
- `QL_NULL_AS_FUNCTIONS=ON`
- `XAD_STATIC_MSVC_RUNTIME=ON`

For Linux, the command-line for this is:

```shell
cd QuantLib
mkdir build
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DQL_EXTERNAL_SUBDIRECTORIES=`pwd`/../../xad;`pwd`/../../qlxad \
  -DQL_EXTRA_LINK_LIBRARIES=qlxad \
  -DQL_NULL_AS_FUNCTIONS=ON \
  -DXAD_STATIC_MSVC_RUNTIME=ON
```

In Windows, you can use the CMake GUI to generate the build files,
setting the same variables as above.

### 5. Building

The generated build files can now be built using the regular native
build tools. For example, in Linux `make` can be run, 
and in Visual Studio, the solution can be opened and built.
Note that we recommend Release mode for Windows builds.

### 6. Running the Tests

There are two test executables that get built - the regular QuantLib
test suite with all the standard tests from the mainline repository,
as well as the QuantLib XAD test suite from the qlxad repository.
Both are using the overloaded XAD type for `double`,
but only the XAD suite checks for the correctness of the derivatives as well.

These executables can simply be run to execute all the tests.
We recommend to use the parameter `--log_level=message` to see the test 
progress.
Alternatively, CTest can also be used to execute them.

### 7. Running the Examples

Apart from the regular QuantLib examples, there are XAD-specific examples
in the qlxad repository, in the `Examples` folder.
These demonstrate the use of XAD to calculate derivatives using AAD.

## Planned Features

- Gradually port more of the QuantLib tests and add AAD-based sensitivity calculation
- Add more Examples


## License

Due to the nature of this repository, two different licenses have to be used for 
different part of the code-base.
The [tests](test-suite/) and [examples](Examples/) folders are containing code taken
and modified from QuantLib where the [QuantLib license](test-suite/LICENSE.TXT] applies.
The [ql](ql/) folder contains adaptor modules for XAD,
where the [GNU AGPL](ql/LICENSE.md) applies.
This is clearly indicated by having separate license files in each folder.
