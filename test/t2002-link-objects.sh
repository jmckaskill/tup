#! /bin/sh -e

# Since *.o isn't 'tup touched', we have to get them from the output of the
# first rule.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc -o prog.exe %f |> prog.exe
HERE
tup touch foo.c bar.c Tupfile
tup parse
tup_object_exist . foo.c bar.c
tup_object_exist . "gcc -c foo.c -o foo.o"
tup_object_exist . "gcc -c bar.c -o bar.o"
tup_object_exist . "gcc -o prog.exe bar.o foo.o"

eotup
