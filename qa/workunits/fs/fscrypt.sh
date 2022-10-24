#!/usr/bin/env bash

set -xe

mydir=`dirname $0`

if [ $# -ne 2 ]
then
	echo "2 parameters are required!"
	exit 1
fi

fscrypt=$1
testcase=$2
testdir=fscrypt_test_${fscrypt}_${testcase}
mkdir $testdir

XFSPROGS_DIR='xfprogs-dev-dir'
XFSTESTS_DIR='xfstest-dev-dir'
export XFS_IO_PROG="$(type -P xfs_io)"

# Setup the xfstests env
setup_xfstests_env()
{
	git clone https://git.ceph.com/xfstests-dev.git $XFSTESTS_DIR --depth 1
	pushd $XFSTESTS_DIR
	. common/encrypt
	popd
}

# Install xfsprogs-dev from source to support "add_enckey" for xfs_io
install_xfsprogs()
{
	local install_xfsprogs=0

	xfs_io -c "help add_enckey" | grep -q 'not found' && install_xfsprogs=1

	if [ $install_xfsprogs -eq 1 ]; then
		git clone https://git.ceph.com/xfsprogs-dev.git $XFSPROGS_DIR --depth 1
		pushd $XFSPROGS_DIR
		make
		sudo make install
		popd
	fi
}

clean_up()
{
	rm -rf $XFSPROGS_DIR
	rm -rf $XFSTESTS_DIR
	rm -rf $testdir
}

# For now will test the V2 encryption policy only and the
# V1 encryption policy is deprecated

install_xfsprogs
setup_xfstests_env

# Generate a fixed keying identifier
raw_key=$(_generate_raw_encryption_key)
keyid=$(_add_enckey $testdir "$raw_key" | awk '{print $NF}')

case ${fscrypt} in
	"none")
		# do nothing for the test directory
		pushd $testdir
		${mydir}/../suites/${testcase}.sh
		popd
		clean_up
		;;
	"unlocked")
		# set encryption policy with the key provided and
		# then the test directory will be encrypted & unlocked
		_set_encpolicy $testdir $keyid
		pushd $testdir
		${mydir}/../suites/${testcase}.sh
		popd
		clean_up
		;;
	"locked")
		# remove the key, then the test directory will be locked
		# and any create/remove will be denied by requiring the key
		_rm_enckey $testdir $keyid
		clean_up
		;;
	*)
		clean_up
		echo "Unknow parameter $1"
		exit 1
esac
