.. _sec-tools:

=======
 Tools
=======

NVMe CLI
========

The `NVMe CLI <https://github.com/linux-nvme/nvme-cli>`_ tool supports
Open-Channel SSDs through the lnvm extension. You may use "nvme lnvm" to see
supported parameters.

... bring up / teardown ...

liblightnvm
===========

... dependency for the following tools

lnvm-tools
==========

... device conditioning ...

nvm_pblk
========

... meta-data checking ...

.. literalinclude: nvm_pblk_usage.out
   :language: none

Check meta-data
---------------

.. literalinclude:: nvm_pblk_mdck.cmd
   :language: bash

.. literalinclude:: nvm_pblk_mdck.out
   :language: bash
