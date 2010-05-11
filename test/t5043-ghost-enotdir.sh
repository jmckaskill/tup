#! /bin/sh -e

# Make sure we can get a ghost if a file is used as a directory (so we get
# ENOTDIR as the error code).

. ./tup.sh
cat > Tupfile << HERE
: |> (cat secret/ghost || echo nofile) > %o |> output.txt
HERE
tup touch secret Tupfile
update
echo nofile | diff -b - output.txt

rm secret
tup rm secret
tmkdir secret
echo 'boo' > secret/ghost
tup touch secret/ghost
update

echo boo | diff -b - output.txt

eotup
