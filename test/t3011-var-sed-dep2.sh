#! /bin/sh -e

# Similar to t3009, only make sure that if the variable is deleted the command
# is still executed.

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

varsetall
update_fail

varsetall FOO=diggity
update
(echo "hey diggity yo") | diff -b out.txt -
(echo "hey diggity yo") | diff -b new.txt -

eotup
