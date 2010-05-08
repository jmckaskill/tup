#! /bin/sh -e

# Make sure a var/sed command ends up propagating down the DAG. When I made the
# sticky link change, it looks like this broke.

. ./tup.sh
cat > Tupfile << HERE
, foo.txt |> out.txt
: out.txt |> cat %f > %o |> new.txt
HERE
echo "hey @FOO@ yo" > foo.txt
tup touch foo.txt Tupfile
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt new.txt
(echo "hey sup yo") | diff -b out.txt -
(echo "hey sup yo") | diff -b new.txt -

echo "a @FOO@ b" > foo.txt
tup touch foo.txt
update
(echo "a sup b") | diff -b out.txt -
(echo "a sup b") | diff -b new.txt -

eotup
