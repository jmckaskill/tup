#! /bin/sh -e

# Apparently a symlink to '.' causes tup to segfault.

. ./tup.sh
unix_only

tmkdir sub
cd sub
ln -s . foo
update

eotup
