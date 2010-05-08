#! /bin/sh -e

# Make sure a symlink doesn't go into the modify list when the monitor starts.
. ./tup.sh
unix_only
check_monitor_supported
tup monitor

mkdir foo-x86
ln -s foo-x86 foo

update
stop_monitor

tup monitor
check_empty_tupdirs

eotup
