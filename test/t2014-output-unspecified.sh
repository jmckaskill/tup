#! /bin/sh -e

# Verify that if a command writes to a file that it wasn't already linked to,
# it explodes. Things can't possibly work if the DAG is incomplete.

. ./tup.sh
cat > Tupfile << HERE
: foo.c |> gcc -c foo.c -o foo.o && touch bar |> foo.o
HERE
touch foo.c
tup touch foo.c Tupfile
update_fail

eotup
