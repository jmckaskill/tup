@echo off

rmdir /S /Q build
mkdir build
mkdir build\ldpreload
mkdir build\cp
cd build

set CFLAGS=/I..\src /I..\src\compat\msvc /I..\src\compat\win32 /nologo /c /D__STDC__ /D_CRT_SECURE_NO_DEPRECATE

FOR %%I IN (..\src\linux\*.c ..\src\tup\*.c ..\src\compat\win32\*.c) DO cl %%I %CFLAGS%

cl %CFLAGS% ..\src\tup\tup\main.c
cl %CFLAGS% ..\src\tup\colors\no_colors.c
cl %CFLAGS% ..\src\tup\monitor\null.c

echo const char *tup_version(void) {return "bootstrap";} > version.c
cl %CFLAGS% version.c

cl %CFLAGS% ..\src\sqlite3\sqlite3.c /DSQLITE_TEMP_STORE=2 /DSQLITE_THREADSAFE=0 /DSQLITE_OMIT_LOAD_EXTENSION
cl %CFLAGS% ..\src\ldpreload\dllinject.c /Fo"ldpreload\dllinject.obj"
cl %CFLAGS% ..\src\cp\main.c /Fo"cp\main.obj"

link /dll /nologo /OUT:"tup-dllinject.dll" ldpreload\dllinject.obj psapi.lib ws2_32.lib user32.lib
link /nologo /OUT:"cp.exe" cp\main.obj
link /nologo /OUT:"tup.exe" *.obj tup-dllinject.lib ws2_32.lib

cd ..

build\tup init
build\tup upd

echo Build complete. If tup works, you can remove the 'build' directory.

