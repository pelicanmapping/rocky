# Overview

This project (Rocky) is a 3D geospatial rendering engine. In other words it renders 3D maps and globes complete with 
imagery, elevation, and GIS feature data.

The underlying rendering engine is VulkanSceneGraph (VSG) which itself is based on Vulkan.

Performance and scalability (e.g., handling huge amounts of data) are of paramout importance.
Geospatial accurancy and precision are also high priority.


# Building

This project uses CMake to build.
Do not attempt to figure out how to build this project on your own.  Use these instructions.

The build folder is (usually) in ../build (but the user MAY have overriden that with the bootstrap script).
Out-of-source builds are preferred.

To configure this project you can run this from the root folder:
```
build/configure.bat
```
This will create an out of source cmake build in a directory called "../build".

To build the project run this command from the root folder:
```
build/build.bat
```
Keep in mind that it could take a long time to build the project from scratch.


# Coding Standards

Code needs to be C++17 compliant.
Code needs to build on various platforms, so don't write code for which MSVC has "relaxed rules".
Indent with 4 spaces. No tabs.
Line break at 128 characters.
New code should use the same EOL style (CRLF versus LF) as the existing code in the same file. When in doubt, or for new files, prefer CRLF.

# Documentation

Document every new function with a concise comment describing its purpose and,
where relevant, its ownership, threading, preconditions, and failure behavior.
Include new helpers and test/benchmark functions; explain non-obvious reasoning
instead of merely restating the function name.