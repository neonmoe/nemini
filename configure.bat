@echo off

if exist Makefile (del Makefile)
echo OUTDIR=build-windows-x86>>Makefile
echo EXE=nemini.exe>>Makefile
echo OBJS=src\main.obj>>Makefile
echo CFLAGS=/Iinclude /TC /Wall /Os /GS /guard:cf /Za /nologo>> Makefile
echo LIBS=/link /subsystem:windows /nologo lib\x86\SDL2main.lib lib\x86\SDL2.lib shell32.lib>>Makefile
echo OUTFLAG=/Fe>>Makefile
echo DIRSEP=\\>>Makefile

echo RMFILE=del /Q>>Makefile
echo RMDIR=rmdir /Q>>Makefile
echo MKDIR=mkdir>>Makefile
echo WINDOWS_DLL_COPYING_STEP=copy lib\x86\SDL2.dll build-windows-x86>>Makefile
echo WINDOWS_DLL_DELETION_STEP=del build-windows-x86\SDL2.dll>>Makefile

type Makefile.in>>Makefile

REM Overwrite the default C compilation rule to ensure that the files
REM end up in src/ as they are expected.
echo {src\}.c{src\}.obj:>>Makefile
echo         $(CC) $(CFLAGS) /c $^< /Fo$@>>Makefile
