#! /bin/sh -e

# Make sure a ghost variable gets removed when necessary.

. ./tup.sh
cat > Tupfile << HERE
file-y = foo.c
file-@(BAR) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
echo hey > foo.c
echo yo > bar.c
tup touch foo.c bar.c Tupfile
update
tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"
tup_object_exist @ BAR

cat > Tupfile << HERE
file-y = foo.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
tup touch Tupfile
update

tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"
tup_object_no_exist @ BAR

eotup
