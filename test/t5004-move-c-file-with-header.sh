#! /bin/sh -e

. ./tup.sh
cp ../testTupfile.tup Tupfile

# Verify both files are compiled
(echo "#include \"foo.h\""; echo "int main(void) {return 0;}") > foo.c
(echo "#include \"foo.h\""; echo "void bar1(void) {}") > bar.c
echo "#define FOO 3" > foo.h
tup touch foo.c bar.c foo.h
update
sym_check foo.o main
sym_check bar.o bar1
sym_check prog.exe main bar1

# Rename bar.c to realbar.c.
mv bar.c realbar.c
tup rm bar.c
tup touch realbar.c
update
check_not_exist bar.o
tup_object_no_exist bar.c bar.o
sym_check realbar.o bar1
sym_check prog.exe main bar1

eotup
