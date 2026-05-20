# Overview
A good overview of this project is in the [README.md](README.md)

# Building
This project uses CMake to build.

Do not attempt to figure out how to build this project on your own.  Use these instructions explicitly.

The build directory contains scripts you can use to build this project.

To configure this project you can run
```
configure.bat
```
This will create an out of source cmake build in a directory called "x64-windows".

To build the project run this command
```
build.bat
```
Keep in mind that it could take a long time to build the project from scratch.

Before running any rocky commands you need to run the rocky_shell.bat script to setup your PATH correctly.

To run unit tests run this command from the tests directory
```
rocky_tests
```

# Demos
Rocky has a project at [src/apps/rocky_demo](src/apps/rocky_demo) that contains many ImGui based demos that highlight capabilities of Rocky and also serves as a visual testing tool.  Each demo is stored in it's own file prefixed with Demo_XXX.h.  If you are asked to create a new demo, follow the coding style and theme of the existing demos and do your work there.