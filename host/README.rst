Secure Gadget Library Client Application Library
################################################

.. contents::

.. section-numbering::

Version
======
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
Client Application Library provides APIs for decoder component; while it has
a corresponded Trusted Application running in Secure OS to assist it provide
the functions.

Directories
==========
.. code-block:: bash

        TOP
        ├── include		header file for external usage
        └── src
            ├── include		header file for internal usage
            ├── memory		memory related operations
            ├── optee		OP-TEE related operations
            └── arm		video related operations
