#! /bin/sh -e

. ./tup.sh
check_monitor_supported
tup monitor
mkdir a
mkdir a/a2
cp ../testTupfile.tup a/a2/Tupfile

echo "int main(void) {return 0;}" > a/a2/foo.c
update
tup_object_exist . a
tup_object_exist a a2
tup_object_exist a/a2 foo.c foo.o prog.exe 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog.exe'
sym_check a/a2/foo.o main
sym_check a/a2/prog.exe main

# Move directory a to b
mv a b
update
tup_object_exist . b
tup_object_exist b a2
tup_object_exist b/a2 foo.c foo.o prog.exe 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog.exe'
tup_object_no_exist . a
tup_object_no_exist a a2
tup_object_no_exist a/a2 foo.c foo.o prog.exe 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog.exe'

eotup
