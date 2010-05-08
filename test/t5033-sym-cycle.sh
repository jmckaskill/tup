#! /bin/sh -e

# sym-cycle: a game about a wheel?
. ./tup.sh
unix_only

ln -s a b
ln -s b a
tup touch a
tup touch b
cat > Tupfile << HERE
: |> if [ -f a ]; then cat a 2>/dev/null; else echo yo; fi > %o |> output.txt
HERE
tup touch Tupfile
update
echo yo | diff -b - output.txt

cat > Tupfile << HERE
: |> echo yoi > %o |> output.txt
HERE
tup touch Tupfile
rm -f a b
tup rm a b
update
echo yoi | diff -b - output.txt
tup_object_no_exist . a b

eotup
