# Building

This project uses CMake to build.
Do not attempt to figure out how to build this project on your own.  Use these instructions explicitly.
The build directory contains scripts you can use to build this project. Run them from the repo's root folder.

To configure this project, enter the repo's root directory, and run
```
build/configure.bat
```
This will create an out of source cmake build in a directory called "../build".

To build the project run this command
```
build/build.bat
```

# Running

Before running any rocky commands you need to run the rocky_shell.bat script to setup your PATH correctly.
Run it with the repo root as your working folder, i.e., "build/rocky_shell.bat".

Set the environment variable to get a reasonable window size:
OSG_WINDOW=1920 1080 20 20

On windows, the dependencies are in ../build/vcpkg_installed under both the x64-windows and x64-windows-release folders. Be sure to include those in your PATH. (These should be set in rocky_shell.bat)

To run unit tests run this command:
```
rocky_tests
```

To run the visual demo application, run this:
```
rocky_demo
```

# Coding

Normalize all line endings to match the development platform's standard.

Code indentation should use 4 spaces (no tabs).

Use ASCII characters ONLY in all code and comments.
