#! /bin/sh -e

echo "CONFIG_TUP_NO_COLORS=y" > tup.config

rm -rf build
echo "  mkdir build"
mkdir -p build
mkdir -p build/ldpreload
echo "  cd build"
cd build

CFLAGS="-D_NO_OLDNAMES -I../src -I../src/compat/win32"

for i in ../src/linux/*.c ../src/tup/*.c ../src/compat/win32/*.c ../src/tup/tup/main.c ../src/tup/monitor/null.c ../src/tup/colors/no_colors.c $plat_files; do
	echo "  bootstrap CC (unoptimized) $i"
	mingw32-gcc -c $i $CFLAGS
done

echo "  bootstrap CC (unoptimized) ../src/sqlite3/sqlite3.c"
mingw32-gcc -c ../src/sqlite3/sqlite3.c -DSQLITE_TEMP_STORE=2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION $CFLAGS

echo "  bootstrap CC (unoptimized) ../src/ldpreload/dllinject.c"
mingw32-gcc -c ../src/ldpreload/dllinject.c -o ldpreload/dllinject.o $CFLAGS

echo "  bootstrap LD.so tup-dllinject.so"
mingw32-gcc -shared -o tup-dllinject.dll ldpreload/dllinject.o -lpsapi -lws2_32

echo "  bootstrap LD tup"
echo "const char *tup_version(void) {return \"bootstrap\";}" | mingw32-gcc -x c -c - -o tup_version.o
mingw32-gcc *.o -o tup tup-dllinject.dll -lpsapi -lws2_32

cd ..
# We may be bootstrapping over an already-inited area.
./build/tup init || true
./build/tup upd
echo "Build complete. If ./tup works, you can remove the 'build' directory."

