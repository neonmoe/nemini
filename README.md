# **Ne**on's Ge**mini** Client

A [Gemini](https://gemini.circumlunar.space/) client written in C with
[SDL 2](https://libsdl.org/) and the [stb
libraries](https://github.com/nothings/stb), for Linux and MS Windows,
as well as presumably everything else¹.

¹ Namely, operating systems with an SDL 2.0 port. Check out the list
[here](https://wiki.libsdl.org/Installation#Supported_platforms).

## Building

All the build files are hand-written and very short, please
familiarize yourself with them if you intend to package this software
for others.

### Linux

Prerequisites:

- A C compiler
- [SDL2](https://wiki.libsdl.org/Installation#Linux.2FUnix)
- make
- pkg-config
- libssl
- libcrypto

Then run:

```sh
./configure
make
```

The executable will be built into the `build-linux-<arch>` directory.

### Windows

Prerequisites:

- [MSVC](https://visualstudio.microsoft.com/)
  - Specifically, you need nmake.exe, cl.exe, and link.exe.
- [SDL2](https://libsdl.org/download-2.0.php)
  - Download the Visual C++ development libraries, and extract the
    `lib` and `include` directories next to this repo's `src`
    directory.

Open a Developer Command Prompt (x86) and run:

```
configure.bat
nmake
```

The configure.bat script should tell you if you're missing any of the
prerequisites. If you want to build a 64-bit client, you need to edit
every mention of x86 to x64 in configure.bat, and then build in a
64-bit Developer Command Prompt.

The executable will be built and the SDL2.dll (from the SDL2
prerequisite step) will be copied into the `build-windows-x86`
directory.

### Other operating systems

This program is (hopefully) written to be portable, and it uses SDL2
in a very straightforward way. Software rendering is supported. For
OS-specific instructions, see [the SDL wiki's installation
instructions](https://wiki.libsdl.org/Installation#Supported_platforms).

## License

This program is distributed under the terms of the [GNU
GPLv3](LICENSE.md) license.
