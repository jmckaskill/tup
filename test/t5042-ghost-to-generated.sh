#! /bin/sh -e

# Make sure changing a ghost node to a generated node works.

. ./tup.sh
cat > Tupfile << HERE
: |> (cat secret/ghost || echo nofile) > %o |> output.txt
HERE
tup touch Tupfile
update
echo nofile | diff -b - output.txt

cat > Tupfile << HERE
: |> echo yo > %o |> secret
HERE
tup touch Tupfile
update

eotup
