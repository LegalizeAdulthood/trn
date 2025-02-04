<!--
[![GitHub Release](https://img.shields.io/github/v/release/LegalizeAdulthood/trn?label=Latest+Release)](https://github.com/LegalizeAdulthood/iterated-dynamics/releases)
-->
[![CMake workflow](https://github.com/LegalizeAdulthood/trn/actions/workflows/cmake.yml/badge.svg)](https://github.com/LegalizeAdulthood/trn/actions/workflows/cmake.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![License](https://img.shields.io/github/license/LegalizeAdulthood/trn?label=License)](https://github.com/LegalizeAdulthood/trn/blob/master/LICENSE.txt)
<a href="https://repology.org/project/trn/versions">
    <img src="https://repology.org/badge/vertical-allrepos/trn.svg"
        alt="Packaging status" align="right" style="padding-left: 20px">
</a>

# Trn, Version 5.0

trn, version 5.0<br/>
Copyright (c) 2023-2025, Richard Thomson

Based on [trn, version 4.0-test77](https://sourceforge.net/projects/trn/)</br>
Copyright (c) 1995, Wayne Davison

Based on rn, Version 4.4</br>
Copyright (c) 1985, Larry Wall</br>
Copyright (c) 1991, Stan Barber

You may copy trn in whole or in part as long as you don't try to
make money off it, or pretend that you wrote it.

See the file INSTALL for installation instructions.  Failure to do so
may void your warranty. `:-)`

# Version 5.0

This is a version of trn-4.0-test77 with patches by [acli](https://github.com/aclu)
to display UTF-8 reasonably correctly.  However, the original “character set”
conversions are currently disabled.  Bugs that have nothing to do with UTF-8
support are also being worked on.
(As acli says, "yes, I’ve tried tin and nn, and [no, tin is not easier and nn is not better](https://github.com/acli/trn/wiki/Project-whiteboard)").

Posting is half-working, with Content-Transfer-Encoding declared as 8bit.
A more proper fix is the next step.

Further along, the original conversions need to be put back in,
but in a way that wouldn’t corrupt UTF-8.


# Where to send bug reports

Use the [github project page](https://github.com/LegalizeAdulthood/trn) to file
bug reports.  Use the `v`ersion command from the newsgroup selection level of trn to be
reminded of this URL and see some configuration information to
send along with your bug report.

# What is trn?

Trn is *t*hreaded [*rn*](https://en.wikipedia.org/wiki/Rn_(newsreader)) -- a newsreader that uses an article's references to
order the discussions in a very natural, reply-ordered sequence called
threads.  Having the replies associated with their parent articles not
only makes following the discussion easier, but also makes it easy to back-
track and (re-)read a specific discussion from the beginning.  Trn also
has a visual representation of the current thread in the upper right corner
of the header, which will give you a feel for how the discussion is going
and how the current article is related to the last one you read.

In addition, a thread selector makes it easy to browse through a large
group looking for interesting articles.  You can even browse through the
articles you've already read and select the one(s) you wish to read again.
Other nice features include the extract commands for the source and binary
groups, thread-oriented kill directives, a better newgroup finding strategy,
and lots more.  See the change log for a list of the things that are new to
trn from previous versions (either view the file [changelog](HelpFiles/changelog)
or type `h` (help) from inside trn and select the "What's New?" entry).

To make trn work faster it uses an auxiliary news
database with overview files, which are maintained by INN.
Trn supports local news groups and news accessed remotely via
[NNTP](https://en.wikipedia.org/wiki/Network_News_Transfer_Protocol).  If you
opt for remote access you will probably want to make the adjunct database
available too.  You can do this in a variety of way, but I recommend that
you send the database from the server to the client via NNTP.

This version supports the XOVER command (to send
overview files), the XTHREAD command (to send thread files), and the XINDEX
command (though trn doesn't support using it).  The alternative is to either
mount the disk containing your database via NFS, or build it locally.  See
the mthreads package for details on how to do this.

Note that trn is based on rn, and so it does a great job of pretending to
be rn for those people that simply don't like to change their newsreading
habits.  It is possible to install trn as both rn and trn linked together
and have it act as both newsreaders, thus saving you the hassle of maint-
aining two separate newsreaders.  A Configuration option allows you to
decide if you want trn to check its name on startup.

# Building

Trn uses [CMake](https://cmake.org) to generate build scripts,
[CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) to run tests
and [vcpkg](https://vcpkg.io) to obtain dependencies.

## Obtaining the Source Code

After cloning the repository from github, initialize and update submodules
to get vcpkg:

```
git clone https://github.com/LegalizeAdulthood/trn.git
cd trn
git submodule init
git submodule update --depth 1
```

## CMake Presets

[CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
are provided to simplify the process of running CMake and CTest.  CMake presets are
defined according to a JSON schema.
Presets can be used to define configure, build, test and packaging steps.  A workflow
preset can group configure, build, test and packaging steps together.
You can create your own presets to configure trn options in the file `CMakeUserPresets.json`.
Trn is distributed with a `default` preset for configure, build, test and workflow in
the file [CMakePresets.json](CMakePresets.json).

## Configure

Configure CMake to generate build scripts:

```
cmake --preset default
```

When trn is configured, vcpkg will download dependency source packages into the vcpkg
directory.  Dependency source packages and build outputs are cached in the vcpkg directory,
so even if you remove your entire build tree, vcpkg will still have the results.

**Note:** the `default` preset creates the build directory as a peer to the source directory,
so look in the parent directory of your source directory for the generated build scripts,
such as IDE projects for Visual Studio.

## Build

Build the code using CMake:

```
cmake --build --preset default
```

## Test

Run the tests using CMake:

```
ctest --preset default
```

## Configure, Build and Test with a Workflow

CMake presets can provide a workflow that will execute the configure, build
and tests steps with a single command:

```
cmake --workflow --preset default
```

# Contributing

Trn welcomes contributions and doesn't impose any restrictions or burdens on
contributors!

Please submit contributions as git pull requests on the
[github project page](https://github.com/LegalizeAdulthood/trn).

The author uses [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/vs/community/)
with [ReSharper for C++](https://www.jetbrains.com/resharper-cpp/) as his primary
development environment.

## Continuous Integration and Testing

Building and testing are automated with [github actions](https://github.com/LegalizeAdulthood/trn/actions).

Trn as initially written didn't have any automated tests.  As changes are
made, best efforts are taken to add new automated tests for bugs found and fixed
and features added.

Please try to add new tests for any bugs fixed or features added.  The legacy code
that is trn 4 doesn't necessarily make such things easy, but consult the existing
body of tests for ideas on how you can add tests for your changes.
