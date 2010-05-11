#! /bin/sh -e

# Make sure execve gets trapped by ldpreload and handled as a 'read'

. ./tup.sh
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo.exe
: foo.exe |> .\\foo.exe |>
HERE

echo "int main(void) {return 0;}" > foo.c
tup touch foo.c Tupfile
update
tup_dep_exist . foo.exe . '.\foo.exe'

eotup
