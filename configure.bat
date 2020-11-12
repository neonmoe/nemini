@echo off

if not exist include (
   echo The include directory is missing! Please extract it - and lib - from the SDL2 Visual C++ development library archive.
   echo Remember to copy the LibreSSL/OpenSSL include\ directory as well. See README.md for further information.
   pause
   if not exist include (
      exit /B
   )
)
if not exist lib (
   echo The lib directory is missing! Please extract it from the SDL2 Visual C++ development library archive.
   echo Remember to copy the LibreSSL/OpenSSL .lib and .dll files as well. See README.md for further information.
   pause
   if not exist lib (
      exit /B
   )
)

if exist Makefile (del Makefile)
echo OUTDIR=build-windows-x86>>Makefile
echo EXE=nemini.exe>>Makefile
echo BITS=x86>>Makefile
echo OBJS=src\main.obj src\net.obj src\socket.obj src\url.obj src\error.obj src\gemini.obj src\text.obj src\str.obj src\browser.obj>>Makefile
echo CFLAGS=/Iinclude /TC /O2 /GS /guard:cf /nologo>> Makefile
echo LIBS=/link /DEBUG:FULL /subsystem:windows /nologo lib\$(BITS)\SDL2main.lib lib\$(BITS)\SDL2.lib lib\$(BITS)\*crypto*.lib lib\$(BITS)\*ssl*.lib shell32.lib Ws2_32.lib>>Makefile
echo OUTFLAG=/Fe>>Makefile
echo DIRSEP=\\>>Makefile

echo RMFILE=del /Q>>Makefile
echo MKDIR=mkdir>>Makefile
echo WINDOWS_COPY_SDL_DLL=copy lib\$(BITS)\SDL2.dll build-windows-$(BITS)>>Makefile
echo WINDOWS_COPY_LIBSSL_DLL=copy lib\$(BITS)\*ssl*.dll build-windows-$(BITS)>>Makefile
echo WINDOWS_COPY_LIBCRYPTO_DLL=copy lib\$(BITS)\*crypto*.dll build-windows-$(BITS)>>Makefile
echo WINDOWS_DELETE_SDL_DLL=$(RMFILE) build-windows-$(BITS)\SDL2.dll>>Makefile
echo WINDOWS_DELETE_LIBSSL_DLL=$(RMFILE) build-windows-$(BITS)\*ssl*.dll>>Makefile
echo WINDOWS_DELETE_LIBCRYPTO_DLL=$(RMFILE) build-windows-$(BITS)\*crypto*.dll>>Makefile

type Makefile.in>>Makefile

REM Overwrite the default C compilation rule to ensure that the files
REM end up in src/ as they are expected.
echo {src\}.c{src\}.obj:>>Makefile
echo         $(CC) $(CFLAGS) /c $^< /Fo$@>>Makefile
