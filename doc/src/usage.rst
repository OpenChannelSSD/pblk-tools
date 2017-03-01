.. _sec-usage:

=======
 Usage
=======

Bring Up / Teardown
===================

Create a pblk instance named ``pblkdev`` using parallel units 0-127:

.. literalinclude:: usage_00.cmd
   :language: bash

The instance ``pblkdev`` is removed by invoking:

.. literalinclude:: usage_01.cmd
   :language: bash

Conditioning (lnvm-tools)
=========================

...

Verification (nvm_pblk)
=======================

...

State and Debugging (sysfs)
===========================

Once the pblk instance is created and the block device available, a sysfs
interface is exposed that allows further communication with the pblk instance.
This interface can be found at /dev/block/$INSTANCE_NAME/pblk/. Some of the
available files are always exposed to the user, while others that require
collecting statistics on the FTL itself, require the debug mode activate.

The following subsections describe each file and groups them by
:ref:`sec-usage-read-files`, :ref:`sec-usage-debug-read-files`
:ref:ref:`sec-usage-control-knobs`, and :ref:`sec-usage-debug-knobs`.

.. _sec-usage-read-files:

Read Files
----------

rate_limiter
~~~~~~~~~~~~

Current state of the rate limiter in terms of the number of in-flight user and
garbage-collection (GC) I/O.

Content example::

	u:53/16384,gc:0/0/0(16384)(stop:<4096,full:>32768,free:130921/136320)

There are 53 entries on the write buffer which contain user I/O data out of
16384 total entries on the buffer. There are 0 entries being used for GC I/O
out of 0 available. This means that GC is not running.

This is because there are 130921 free blocks out of 136320 total blocks on the
device, and GC does not run until the 32768 free block threshold is reached. At
<4096 free blocks, user I/O stops.

.. NOTE :: All these parameters are configurable

write_buffer
~~~~~~~~~~~~

Current state of the write buffer.

Content example::

	16384 53 16 16 64 0 4294967295 - 53/16383/0 – 0

Total entries: 16384, mem:53, subm:16, sync:16, l2p_update:64, sync_point:0,
number of entries being used:53, number of free entries:16330, entries to be
flushed:0, entries queued on completion: 0.

lines
~~~~~

Lines lists as well as the metadata for the current data line.

Content example::

	lines GC: full:12, high:1, mid:0, low:5, empty:2 data (21) cur:16,
	left:523248/523248, vsc:523248, s:523248, map:0/524288 (0)

There are 12 lines with all sectors invalidated, 1 line with a high invalid
sector count, 5 lines with a low sector count and 2 lines where all sectors are
valid. The current data line is line 21. The write pointer is at sector 16 and
there are 523248 sectors left out of 523248 (line is new).

The valid sector count (vsc) and the number of sectors in line is also 523248.
Since the line is empty, no sectors have been map (0/523248). Finally, the line
is using the metadata line 0 (out of 2 available).

lines_info
~~~~~~~~~~

General information about all data lines.

Content example::

	smeta - len:65536, secs:16
	emeta - len:4194304, sec:1024, bb_start:64
	bitmap lengths: sec:65536, blk:16, lun:16
	blk_line:128, sec_line:524288, sec_blk:4096

The metadata placed at the beginning of the line occupies 16 sectors (length
65536 bytes). The metadata placed at the end of the line occupies 1024 sectors
(length 4194304 bytes), and it spreads across 64 blocks. The bitmaps used for
sectors are 65536 bytes long and for blocks and LUNs 16 bytes long. Each line
is formed by 128 blocks. Each block has 4096 sectors, which translates into
524288 sectors per line.

ppa_format
~~~~~~~~~~

pblk ppa format and the ppa format for the device in terms of offset and
length.

Content example::

	g:blk:19/13,pg:11/8,lun:8/3,ch:4/4,pl:2/2,sec:0/2
	d:blk:19/13,pg:11/8,lun:8/3,ch:0/4,pl:6/2,sec:4/2

pblk-format
	block (13 bits) – page (8 bits) – LUN (3 bits) – channel (4 bits) – plane (2 bits) – sector (2 bits)

device-format
	block (13 bits) – page (8 bits) – LUN (3 bits) – plane (2 bits) – sector (2 bits) – channel (4 bits)

errors
~~~~~~

Accumulated counts of global errors that occurred since the pblk target was
instantiated.

Content example::

	read_failed=0, read_high_ecc=0, read_empty=0, read_failed_gc=0, write_failed=0, erase_failed=0

write_luns
~~~~~~~~~~

LUNs forming the pblk instance, its order and their state.

Content example::

	pblk: pos:106, ch:10, lun:6 – 0

The LUN in the position 106 corresponds to channel 10, LUN 6. The LUN is not
busy.

gc_state
~~~~~~~~

Shows the state of the GC thread.

Content example::

	gc_enabled=1, gc_active=0

The GC thread is enabled but it is not active (no lines are being garbage
collected).


.. _sec-usage-debug-read-files:

Debug Read Files
----------------

The files described in this sections are only available when the kernel is
compiled with ``CONFIG_NVM_DEBUG``.

stats
~~~~~

Detailed stats about the pblk instance.

Content example::

	0 0 1659328 0 0 1659328 1659328 1659328 0 0 0 0 0 693

Inflight writes:0, inflight reads:0, write request:1659328, flush requests:0,
padded sectors:0, submitted writes:1659328, synced writes:1659328, completed
writes:1659328, inflight metadata I/Os:0, completed meta I/Os:0, recovery
writes:0, GC writes:0, requeued writes:0, synced reads: 693

write_buffer_vb
~~~~~~~~~~~~~~~

Show state of each entry in the write buffer.

Content example::

	entry:8161 - flags:512

Entry 8161 is a writable entry. The following flags can be present in the write
buffer and encoded as::
	# I/O Type
	PBLK_IOTYPE_USER = 1
	PBLK_IOTYPE_GC = 2
	# Buffer State
	PBLK_FLUSH_ENTRY = 1 << 6 (64)
	PBLK_WRITTEN_DATA = 1 << 7 (128)
	PBLK_SUBMITTED_ENTRY = 1 << 8 (256)
	PBLK_WRITABLE_ENTRY = 1 << 9 (512)


.. _sec-usage-control-knobs:

Control Knobs
-------------

gc_force
~~~~~~~~

Force the GC to run, independently of the number of free lines.

Usage example::

	echo “1” > /sys/block/pblkdev/pblk/gc_force

Forces garbage collection routine to run.

.. _sec-usage-debug-knobs:

Debug Knobs
-----------

The files described in this section are only available when the kernel is
compiled with ``CONFIG_NVM_DEBUG``.

l2p_map
~~~~~~~

Query the L2P table for the specified LBA range.

Usage example::

	echo “0-1023” > /sys/block/pblkdev/pblk/l2p_map

lba:0(oob:0) - ppa: 300000000020002: ch:3,lun:0,blk:2,pg:2,pl:0,sec:0 LBA 0 is
mapped to ppa 0x300000000020002 (ch:3,lun:0,blk:2,pg:2,pl:0,sec:0). Besides
querying the L2P map, a read command is sent to the device and the result of
the out-of-bound area is showed too. Note that the out-of-bound area contains
the LBA to which the physical sector is mapped to.

l2p_sanity
~~~~~~~~~~

Check sanity of the L2P table for a given PPA address on a LBA range.

.. NOTE:: At a given point in time, a PPA address should only be mapped to a
	single LBA address. If a PPA is mapped to several LBAs then a corruption on the
	L2P table has occurred.

Usage example (``$PPA-$LBA_BEGIN-$LBA_END``)::

	echo "0-0-10" > /sys/block/pblkdev/pblk/l2p_sanity

Provides no output on success.

In case a corruption is found, the output will be similar to::

	lba:0(oob:0) - ppa: 300000000020002: ch:3,lun:0,blk:2,pg:2,pl:0,sec:0
	lba:0(oob:0) - ppa: 300000000020003: ch:3,lun:0,blk:2,pg3,pl:0,sec:0

Which translates into: LBA 0 is mapped to two different PPAS: 0x300000000020002
and 0x300000000020003.

line_metadata
~~~~~~~~~~~~~

Query the metadata stored on the media for the given line.

Usage example (``$LINE_ID-$CMD``)::

	echo "0-0" > /sys/block/test/pblk/line_metadata

The ``$CMD`` can be one of the following:

PBLK_SEE_ONLY_LINE_META
	0: Print only the metadata
PBLK_SEE_META_AND_LBAS
	1: Print the metadata and the LBA list (L2P portion for line)
PBLK_SEE_META_AND_LBAS_F
	3: Print metadata and force to print the LBA list even if CRC checks fail

line_bb
~~~~~~~

Query bad block metadata for the given line

Usage example::

	echo "0" > /sys/block/test/pblk/line_bb

Provides an output similar to::

	line 0, nr_bb:0, smeta:8, emeta:1024/0

Line 0 contains 0 bad blocks; smeta takes 8 sectors; emeta takes 1024 sectors
and there are 0 bad blocks that affect emeta.

.. NOTE :: emeta is typically spread along the last page of all the blocks on
	the line. If a block is bad, then a block will contain 2 pages of metadata
