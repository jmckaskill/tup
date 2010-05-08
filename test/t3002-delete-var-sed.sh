#! /bin/sh -e

# Test using a var to sed a file

. ./tup.sh
cat > Tupfile << HERE
, foo.txt |> out.txt
HERE
echo "hey @FOO@ yo" > foo.txt
echo "This is an email@address.com" >> foo.txt
tup touch foo.txt Tupfile
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt
tup_object_exist . ", foo.txt > out.txt"
(echo "hey sup yo"; echo "This is an email@address.com") | diff -b out.txt -

cat > Tupfile << HERE
HERE
tup touch Tupfile
update

tup_object_no_exist . out.txt
tup_object_no_exist . ", foo.txt > out.txt"

eotup
