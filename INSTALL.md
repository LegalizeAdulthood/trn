<!-- Copyright (c) 2026, Richard Thomson -->

# Installing trn 5.0

`INSTALL.md` is the canonical install document for trn 5.0.  The
checked-in CMake presets are the supported build entry points.

## Supported Platforms

The 5.0 target platform set is Windows, Linux, FreeBSD, NetBSD, OpenBSD,
and macOS.  Current CI covers Windows, Linux, FreeBSD, NetBSD, and
OpenBSD.  macOS support is a separate 5.0 release gate.

## Required Tools

Install these first:

- Git, with submodule support.
- CMake 3.23 or newer.
- A C++17 compiler and a CMake-supported build tool.
- Bison on Unix-like systems.  On Windows, CMake downloads winflexbison
  during configure.
- Network access for the first vcpkg dependency restore.

On Windows, Visual Studio 2022 with the Desktop development with C++
workload is the expected compiler environment.  On Unix-like systems, use
the platform compiler with Ninja or Make.

## Fresh Checkout

Clone the repository, then initialize submodules:

```console
git clone https://github.com/LegalizeAdulthood/trn.git
cd trn
git submodule update --init --recursive
```

Bootstrap the vcpkg submodule before configuring.

Windows:

```console
.\vcpkg\bootstrap-vcpkg.bat
```

Unix-like systems:

```console
./vcpkg/bootstrap-vcpkg.sh
```

The CMake preset uses `vcpkg/scripts/buildsystems/vcpkg.cmake`, and vcpkg
installs the manifest dependencies from `vcpkg.json` during configure.

## Configure, Build, And Test

The default preset writes its build tree to `../build-default` and builds
the Debug configuration.

Run the steps separately:

```console
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Or run the checked-in workflow preset:

```console
cmake --workflow --preset default
```

## Install

Install the configured build tree with:

```console
cmake --install ../build-default --config Debug
```

Add `--prefix <directory>` to stage the install tree somewhere other than
the configured install prefix.

## Package

No package preset or CPack target is defined yet.  Until the 5.0 package
work lands, there is no supported default package command.  Use the
install command above to stage files from a fresh checkout.

## Runtime Setup

The old install flow asked installers to choose how trn should read news.
That choice still belongs in runtime configuration:

- News can come from an NNTP server or from a local spool.
- Overview data normally comes from the server or local spool.
- Groups can be read unthreaded when no overview data is available, with
  slower startup.
- Posting helpers and install layout are finished by later 5.0 release
  work.

After installation, run `trn` and use the online options editor to save
personal defaults in `.trn/trnrc`.  Site defaults belong in the installed
INIT file once the install layout work is complete.
