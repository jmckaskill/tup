#! /bin/sh -e

. ./tup.sh
cp ../testTupfile.tup Tupfile

echo "int main(void) {} void foo(void) {}" > foo.c
tup touch foo.c Tupfile
update
sym_check foo.o foo
sym_check prog.exe foo

cat Tupfile | sed 's/prog.exe/newprog.exe/g' > tmpTupfile
mv tmpTupfile Tupfile
tup touch Tupfile
update

sym_check newprog.exe foo
check_not_exist prog.exe
tup_object_no_exist . "gcc foo.o -o prog.exe"
tup_object_no_exist . prog.exe

eotup
