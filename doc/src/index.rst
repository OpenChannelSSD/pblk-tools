============================================
 pblk: Host-based FTL for Open-Channel SSDs
============================================

The Physical Block Device (pblk) implements a sector-based fully associative,
host-based FTL that exposes a traditional block I/O interface. Pblk’s main
responsibilities are:

 * Map logical addresses onto physical addresses (4KB granularity) in the L2P
   table
 * Maintain data integrity and data consistency of the L2P table as well as its
   recovery under power-off and power outage
 * Manage controller- and media-specific constrains
 * Handle write and erase errors
 * Implement garbage collection (GC) routines to garbage collect blocks
 * Implement padding when flush is issued from file-systems in order to
   maintain data consistency

Pblk integrates with the LightNVM subsystem. As such, pblk is initialized with
a user-defined range of LUNs. A single device can be carved out and shared
among different pblk instances. Each instance owns a portion of the device’s
capacity and leverages a portion of the device’s throughput. Each instance
manages different parallel units on the device and the assumption is that they
do not interfere with each other when I/Os reach the device.

Here is what you need to take pblk for a spin:

 * An :ref:`sec-ocssd`
 * :ref:`sec-os`
 * Management :ref:`sec-tools`

The following sections will guide you through what you need, if you already
have those in place then jump to the :ref:`sec-usage` section.

Contents:

.. toctree::
   :maxdepth: 2
   :includehidden:

   ocssd.rst
   os.rst
   tools.rst
   usage.rst

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

