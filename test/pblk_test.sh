#!/usr/bin/env bash

function pblk_test_fs () {

# E.g. "nvme0n1"
DEV_NAME=$1
if [ -z "$DEV_NAME" ]; then
	echo "# FAILED: dev_name($DEV_NAME)"
	exit 1
fi
DEV_PATH="/dev/$DEV_NAME"

# E.g. "pblk0"
PBLK_NAME=$2
if [ -z "$PBLK_NAME" ]; then
	echo "#FAILED: pblk_name($PBLK_NAME)"
	exit 1
fi
PBLK_PATH="/dev/$PBLK_NAME"
PBLK_PART_PATH="${PBLK_PATH}p1"

BLUN=$3
if [ -z "$BLUN" ]; then
	echo "#FAILED: blun($BLUN)"
fi

ELUN=$4
if [ -z "$ELUN" ]; then
	echo "#FAILED: elun($ELUN)"
fi

FS_NAME="${PBLK_NAME}fs"
FS_PATH="/tmp/$FS_NAME"

FILES="1 2 4 8 16 32 64 128 256"
FILES_OVERWRITE=1

if [ ! -e $PBLK_PATH ]; then
	echo "# Creating pblk instance -d $DEV_NAME -n $PBLK_NAME -b $BLUN -e $ELUN"

	NVME_OUT=$(nvme lnvm create -d $DEV_NAME -n $PBLK_NAME -t pblk -b $BLUN -e $ELUN)
	if [ "$?" -ne 0 ]; then
		echo "# FAILED: exiting"
		exit 1
	fi
fi

if [ ! -e $PBLK_PART_PATH ]; then
	echo "# Creating partition table"
FDISK_OUT=$(echo "n
p
1


w
" | fdisk $PBLK_PATH)
	if [ "$?" -ne 0 ]; then
		echo "# FAILED: creating partition table"
		exit 1
	fi
fi

if [ ! -e $FS_PATH ]; then
	echo "# Creating mountpoint, filesystem, and mounting"

	MKDIR_OUT=$(mkdir $FS_PATH)
	if [ "$?" -ne 0 ]; then
		echo "# FAILED: mkdir"
		exit 1
	fi

	MKFS_OUT=$(mkfs -t ext4 $PBLK_PART_PATH)
	if [ "$?" -ne 0 ]; then
		echo "# FAILED: creating file system"
		exit 1
	fi

	MOUNT_OUT=$(mount $PBLK_PART_PATH $FS_PATH)
	if [ "$?" -ne 0 ]; then
		echo "# FAILED: mounting file system"
		exit 1
	fi
fi

for FILE in $FILES
do
	FILE_PATH="$FS_PATH/$FILE.rnd"
	if [ ! -e $FILE_PATH ] || [ "$FILES_OVERWRITE" -eq 1 ]; then
		echo "# Writing $FILE_PATH"
		DD_OUT=$(dd if=/dev/zero of=$FILE_PATH bs=1M count=$FILE)
		if [ "$?" -ne 0 ]; then
			echo "# FAILED: dd .. $FILE_PATH"
		fi
	fi
done

sync

}

