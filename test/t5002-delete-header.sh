#! /bin/sh -e

. ./tup.sh
cp ../testTupfile.tup Tupfile

(echo "#include \"foo.h\""; echo "int main(void) {return 0;}") > foo.c
(echo "#include \"foo.h\""; echo "void bar1(void) {}") > bar.c
echo "" > foo.h
tup touch foo.c bar.c foo.h
update
sym_check foo.o main
sym_check bar.o bar1
sym_check prog.exe main bar1

# When the header is removed, the files should be re-compiled. Note we aren't
# doing a 'tup touch foo.c' here
rm foo.h bar.o foo.o
tup rm foo.h
echo "int main(void) {return 0;}" > foo.c
echo "void bar1(void) {}" > bar.c
update
sym_check foo.o main
sym_check bar.o bar1

eotup
