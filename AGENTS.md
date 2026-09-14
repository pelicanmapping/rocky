This project uses CMake to build.

Do not attempt to figure out how to build this project on your own.  Use these instructions explicitly.

The build directory contains scripts you can use to build this project.

To configure this project, enter the top repo directory, and run
```
build/configure.bat
```
This will create an out of source cmake build in a directory called "../build".

To build the project run this command
```
build/build.bat
```
Keep in mind that it could take a long time to build the project from scratch.

Before running any rocky commands you need to run the rocky_shell.bat script to setup your PATH correctly.

To run unit tests run this command from the tests directory
```
rocky_tests
```