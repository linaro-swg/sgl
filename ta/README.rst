Secure Gadget Library Trusted Application
#########################################

.. contents::

.. section-numbering::

Version
=======
``1.0``

License
=======
The software is provided under a BSD-2-Clause license.

About
=====
Secure Gadget Library is part of Arm TrustZone Media Protection reference
implementation. It provides these functions:
* Allocate / free protected media input buffer for decoder.
* Load secure firmware into protected firmware runtime memory for firmware based hardware decoders.

Secure Gadget Library is designed based on Secure OS / Rich OS architecture.
Trusted Application assists Client Library in providing security functions.

Directories
===========
.. code-block:: bash

        TOP
        ├── arm
        │   └── mve		- secure firmware format parser
        ├── include		- Trusted Application header for Client Application
        └── optee		- Main source for OP-TEE OS implementation
