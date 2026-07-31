#!/usr/bin/env bash

# test setfattr remove, and check values of vxattr
# after remove for vxattr, where possible.

set -ex

mkdir -p dir

# ceph.dir.pin, ceph.dir.pin.distributed, and ceph.dir.pin.random
# have been removed. Verify they are no longer recognized.
getfattr -n ceph.dir.pin dir 2>&1 | grep "No such attribute"
setfattr -n ceph.dir.pin -v 1 dir 2>&1 | grep "No such attribute"
# test -x (remove) for removed attr
setfattr -x ceph.dir.pin dir 2>&1 | grep "No such attribute"

getfattr -n ceph.dir.pin.distributed dir 2>&1 | grep "No such attribute"
setfattr -n ceph.dir.pin.distributed -v 1 dir 2>&1 | grep "No such attribute"
setfattr -x ceph.dir.pin.distributed dir 2>&1 | grep "No such attribute"

getfattr -n ceph.dir.pin.random dir 2>&1 | grep "No such attribute"
setfattr -n ceph.dir.pin.random -v 0.01 dir 2>&1 | grep "No such attribute"
setfattr -x ceph.dir.pin.random dir 2>&1 | grep "No such attribute"

#ceph.quota, def value 0, reset val 0
setfattr -n ceph.quota.max_bytes dir 2>&1 | grep "setfattr: dir: Invalid argument"
setfattr -n ceph.quota.max_bytes -v 100000000 dir
#getfattr -n ceph.quota.max_bytes dir | grep 'ceph.quota.max_bytes="100000000"'
setfattr -x ceph.quota.max_bytes dir
setfattr -n ceph.quota.max_files dir 2>&1 | grep "setfattr: dir: Invalid argument"
setfattr -n ceph.quota.max_files -v 10000 dir
#getfattr -n ceph.quota.max_files dir | grep 'ceph.quota.max_files="10000"'
setfattr -x ceph.quota.max_files dir

rmdir dir

echo OK

