#!/usr/bin/env bash
if [ $UID -ne 0 ] ; then
	echo "# FAILED: Must run as root / sudo"
	exit 1
fi

DEV_NAME="nvme0n1"
DEV_PATH="/dev/$DEV_NAME"

PBLK_NAME="pblk0"
PBLK_PATH="/dev/$PBLK_NAME"
PBLK_PART_PATH="${PBLK_PATH}p1"

FS_NAME="pblk0fs"
FS_PATH="/tmp/$FS_NAME"

FILES="1 2 4 8 16 32 64 128 256"
FILES_OVERWRITE=0

MD_NAME="${DEV_NAME}_${PBLK_NAME}.meta"
MD_PATH="/tmp/$MD_NAME"

# probe..
GEO=`nvm_dev geo $DEV_PATH`
if [ "$?" -ne 0 ]; then
	echo "# FAILED: probing device, exiting"
	exit 1
fi

# Grab geometry
NCHANNELS=$(nvm_dev geo $DEV_PATH | grep "nchannels" | cut -d ":" -f 2)
NLUNS=$(nvm_dev geo $DEV_PATH | grep "nluns" | cut -d ":" -f 2)
TLUNS=$(( $NCHANNELS * $NLUNS ))
ELUN=$(( $TLUNS - 1 ))

if [ ! -e $PBLK_PATH ]; then
	echo "# Creating pblk instance"

	NVME_OUT=$(nvme lnvm create -d $DEV_NAME -n $PBLK_NAME -t pblk -b 0 -e $ELUN)
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
		DD_OUT=$(dd if=/dev/urandom of=$FILE_PATH bs=1M count=$FILE)
		if [ "$?" -ne 0 ]; then
			echo "# FAILED: dd .. $FILE_PATH"
		fi
	fi
done

sync

echo "# Reading meta to $MD_PATH"
nvm_pblk mdck $DEV_PATH 2>&1 | tee -a $MD_PATH

echo "# meta is available at $MD_PATH"
