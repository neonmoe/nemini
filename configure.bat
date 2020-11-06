@echo off

if not exist include (
   echo The include directory is missing! Please extract it - and lib - from the SDL2 Visual C++ development library archive.
   pause
   if not exist include (
      exit /B
   )
)
if not exist lib (
   echo The lib directory is missing! Please extract it from the SDL2 Visual C++ development library archive.
   pause
   if not exist lib (
      exit /B
   )
)

if exist Makefile (del Makefile)
echo OUTDIR=build-windows-x86>>Makefile
echo EXE=nemini.exe>>Makefile
echo OBJS=src\main.obj src\net.obj src\socket.obj src\url.obj src\error.obj src\gemini.obj>>Makefile
echo CFLAGS=/Iinclude /TC /O2 /GS /guard:cf /nologo>> Makefile
echo LIBS=/link /subsystem:windows /nologo lib\x86\SDL2main.lib lib\x86\SDL2.lib lib\x86\libcrypto*.lib lib\x86\libssl*.lib shell32.lib Ws2_32.lib>>Makefile
echo OUTFLAG=/Fe>>Makefile
echo DIRSEP=\\>>Makefile

echo RMFILE=del /Q>>Makefile
echo MKDIR=mkdir>>Makefile
echo WINDOWS_COPY_SDL_DLL=copy lib\x86\SDL2.dll build-windows-x86>>Makefile
echo WINDOWS_COPY_LIBSSL_DLL=copy lib\x86\libssl*.dll build-windows-x86>>Makefile
echo WINDOWS_COPY_LIBCRYPTO_DLL=copy lib\x86\libcrypto*.dll build-windows-x86>>Makefile
echo WINDOWS_DELETE_SDL_DLL=$(RMFILE) build-windows-x86\SDL2.dll>>Makefile
echo WINDOWS_DELETE_LIBSSL_DLL=$(RMFILE) build-windows-x86\libssl*.dll>>Makefile
echo WINDOWS_DELETE_LIBCRYPTO_DLL=$(RMFILE) build-windows-x86\libcrypto*.dll>>Makefile

type Makefile.in>>Makefile

REM Overwrite the default C compilation rule to ensure that the files
REM end up in src/ as they are expected.
echo {src\}.c{src\}.obj:>>Makefile
echo         $(CC) $(CFLAGS) /c $^< /Fo$@>>Makefile
