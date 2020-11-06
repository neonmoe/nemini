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

### Linux, probably other POSIXy systems

Prerequisites:

- A POSIX.1-2001 compliant system.
  - I think. getaddrinfo() is needed, is what I mean here.
  - I imagine this also implies that you have libc, but in case it
    doesn't, string.h is also used in the program, so get yourself a
    libc.
- A C compiler
- make
- [SDL2](https://wiki.libsdl.org/Installation#Linux.2FUnix)
- pkg-config ([pkgconf](http://pkgconf.org/),
  [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/))
- libssl ([LibreSSL](https://www.libressl.org/),
  [OpenSSL](https://www.openssl.org/))
- libcrypto ([LibreSSL](https://www.libressl.org/),
  [OpenSSL](https://www.openssl.org/))

Then run:

```sh
./configure
make
```

The executable will be built into the `build-linux-<arch>` directory.

#### Example package configurations

Feel free to send patches containing setups for your favorite distro,
so others can benefit from your working setup.

- Arch Linux:
  - `core/gcc` (build dependency)
  - `core/make` (build dependency)
  - `extra/sdl2`
  - `core/openssl`
  - `core/glibc`

### Windows

Prerequisites:

- [MSVC](https://visualstudio.microsoft.com/)
  - Specifically, you need nmake.exe, cl.exe, and link.exe. Install
    Visual Studio and those should be included.
- [SDL2](https://libsdl.org/download-2.0.php)
  - Download the Visual C++ development libraries, and extract the
    `lib` and `include` directories next to this repo's `src`
    directory.
- [LibreSSL](https://www.libressl.org/)
  - The [OpenBSD FTP
    server](https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/) has windows
    builds up to version 2.5.5 as of writing this, but download
    whichever is the newest version you can find. The windows builds
    are named `libressl-x.x.x-windows.zip`.
  - Extract the `x86` and `x64` folders into your `lib` folder (which
    should be next to `src`, from the SDL2 step).
  - The `include` folder should go next to `src`, just like SDL2.

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

This program is distributed under the terms of the [GNU GPL 3.0 (or
later)](LICENSE.md) license.
