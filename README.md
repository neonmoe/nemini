# **Ne**on's Ge**mini** Client

A [Gemini](https://gemini.circumlunar.space/) client written in C with
[SDL 2](https://libsdl.org/) and the [stb
libraries](https://github.com/nothings/stb), for Linux and MS Windows,
as well as presumably everything else¹.

If you think this project sounds interesting, you should check out
[Lagrange](https://gmi.skyjake.fi/lagrange/) as well. It's a very
similar client, built in C with SDL, and has support for Gopher!

¹ Namely, operating systems with an SDL 2.0 port. Check out the list
[here](https://wiki.libsdl.org/Installation#Supported_platforms).

## Security considerations

[stb_truetype.h](src/stb_truetype.h) is used for text rendering. As it
mentions in a comment at the start, it doesn't check offsets found in
the font files it reads, which could lead to this program reading
arbitrary memory. TODO: Provide a way to set the default font.

As a general consideration, you should probably avoid handling
sensitive data with this application: while it is written with care,
it is also the first relatively complicated C program I've ever
written, so there probably are vulnerabilities all over the place.

## Building

All the build files are hand-written and very short, so feel free to
study them if you come across issues.

### Linux, probably other POSIX systems

Prerequisites:

- A C compiler
- make
- pkg-config ([pkgconf](http://pkgconf.org/),
  [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/))
- A POSIX.1-2001 compliant system for getaddrinfo().
- [SDL2](https://wiki.libsdl.org/Installation#Linux.2FUnix)
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

#### TCC notes

Nemini can be built with [tcc](https://bellard.org/tcc/)! With slight
tweaks. Add `-DSDL_DISABLE_IMMINTRIN_H` to the CFLAGS variable in your
Makefile, and change `-pthreads` in the LIBS variable to
`-lpthreads`. To use tcc as your compiler, just set the CC environment
variable, as you'd assume: `CC=tcc make`.

### Windows

Prerequisites:

- [MSVC](https://visualstudio.microsoft.com/)
  - Specifically, you need nmake.exe, cl.exe, and CMake. Install
    the following from Visual Studio Installer:
  - The newest package named: `MSVC ... build tools ...`
  - `C++ CMake Tools for Windows` (Needed to build LibreSSL below)
- [SDL2](https://libsdl.org/download-2.0.php)
  - Download the Visual C++ development libraries, and extract the
    `lib` and `include` directories next to this repo's `src`
    directory.
- [LibreSSL](https://www.libressl.org/)
  - The newest windows binaries released for LibreSSL (2.2.5, as I'm
    writing this) are too old to support the TOFU verification this
    program has, so you'll need to build LibreSSL
    yourself. Thankfully, it's pretty easy.
  - Download the newest tarball (`libressl-x.x.x.tar.gz`) from
    e.g. [OpenBSD's ftp
    server](https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/), extract it
    somewhere, then build it in the Developer Command Prompt:

    ```
    mkdir build-nmake
    cd build-nmake
    cmake -G "NMake Makefiles" -DBUILD_SHARED_LIBS=ON ..
    nmake
    ```

  - Copy `crypto-<number>.dll` and `crypto-<number>.lib` from the
    `build-nmake\crypto\` directory into the `lib\x86\` directory
    where the SDL files are.
  - Copy `ssl-<number>.dll` and `ssl-<number>.lib` from the
    `build-nmake\ssl\` directory into the `lib\x86\` directory
    where the SDL files are.
  - Finally, copy the `include` directory from the libressl root
    directory (same dir as `build-nmake`) to this directory, similar
    to the SDL `include` directory.

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
