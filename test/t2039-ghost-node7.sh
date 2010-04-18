#! /bin/sh -e

# Make sure ghost nodes in subdirectories work. I had a small bug where I was
# selecting against (dt, name) and if that failed insertting into (newdt, name).
# I ended up with a bunch of duplicate nodes. Here I check for that by making
# two scripts dependent on the same ghost node, then creating the ghost. Both
# scripts should update.

. ./tup.sh

tmkdir include
cat > ok.sh << HERE
if [ -f include/ghost ]; then cat include/ghost; else echo nofile; fi
HERE
cat > Tupfile << HERE
: |> sh ok.sh > %o |> output.txt
: |> sh foo.sh > %o |> foo-output.txt
HERE
chmod +x ok.sh
cp ok.sh foo.sh
tup touch ok.sh foo.sh Tupfile
update
echo nofile | diff -b output.txt -
echo nofile | diff -b foo-output.txt -

echo 'alive' > include/ghost
tup touch include/ghost
update
echo alive | diff -b output.txt -
echo alive | diff -b foo-output.txt -

eotup
