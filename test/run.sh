#!/usr/bin/env bash

DEV_NAME="nvme0n1"
DEV_PATH="/dev/$DEV_NAME"

PBLK_NAME="pblk0"
PBLK_PATH="/dev/$PBLK_NAME"

FS_NAME="pblk0fs"
FS_PATH="/tmp/$FS_NAME"

FILES="1 2 4 8 16 32 64 128 256"
FILES_OVERWRITE=0

# Grab geometry
NCHANNELS=$(nvm_dev geo $DEV_PATH | grep "nchannels" | cut -d ":" -f 2)
NLUNS=$(nvm_dev geo $DEV_PATH | grep "nluns" | cut -d ":" -f 2)
TLUNS=$(( $NCHANNELS * $NLUNS ))
ELUN=$(( $TLUNS - 1 ))

# Create pblk instance, filesystem and fill some data into it
if [ ! -e $PBLK_PATH ]; then
	echo "# Creating pblk instance"
	sudo nvme lnvm create -d $DEV_NAME -n $PBLK_NAME -t pblk -b 0 -e $ELUN
fi

if [ ! -e $FS_PATH ]; then
	mkdir $FS_PATH
	sudo mkfs -t ext4 $PBLK_PATH
	sudo mount $PBLK_PATH $FS_PATH
fi

for FILE in $FILES
do
	FILE_PATH="$FS_PATH/$FILE.rnd"
	if [ ! -e $FILE_PATH ] || [ "$FILES_OVERWRITE" -eq 1 ]; then
		echo "# Writing $FILE_PATH"
		sudo dd if=/dev/urandom of=$FILE_PATH bs=1M count=$FILE
	fi
done

sync

nvm_pblk mdck $DEV_PATH
