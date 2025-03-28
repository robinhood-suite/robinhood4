.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

Base API and Extensibility
==========================

Introduction
------------

The RobinHood policy engine provides a minimal yet powerful base API designed to
define automated data management policies. This API includes core building blocks
such as actions, triggers, and conditions, which can be combined declaratively to
express a wide range of policy behaviors.

Policies using the base API can be written entirely declaratively, without
requiring any knowledge of Python. Administrators can configure standard
behaviors by referencing built-in functions and expressions, keeping policy
definitions readable and simple.

However, the RobinHood policy engine is also designed with extensibility in mind.
When the built-in mechanisms are not sufficient, for example, if a site requires
a custom archival strategy, a new kind of trigger, or an advanced condition to
match files, users can extend the engine by writing Python.

These custom extensions can define new actions, new trigger types, or new
conditions, and they can be seamlessly integrated into the syntax of existing
policy definitions. Because the RobinHood policy engine is modular, adding new
behavior does not require modifying the engine’s core, nor understanding its
internal architecture. Instead, administrators can focus on implementing
precisely the logic they need, in isolation.

This section presents the core components available in the base API and explains
how the policy engine allows users to extend these components to express
site-specific behaviors.

Built-in Actions
----------------

Built-in Actions
================

The RobinHood policy engine provides a set of built-in actions that can be used
directly in policy definitions. These cover common data management operations
such as deleting files, copying data, logging events, and compressing content.
Built-in actions are implemented in Python and can be used without writing any
code, simply by referencing them in the policy configuration.

Each built-in action accepts a set of parameters, some of which may be mandatory
depending on the action's behavior.

Common Actions
--------------

**unlink**: Removes a single entry from the filesystem. This corresponds to the
``unlink(2)`` system call. No specific parameters are required, but the entry
path must be known.

**rmdir**: Removes an empty directory, equivalent to the ``rmdir(2)`` system
call. No specific parameters are required, but the file path must be known.

**copy**: Copies a file from its current location to a specified destination.

Parameters:
- ``targetpath`` (mandatory): Destination path for the copy.
- ``nosync`` (optional): If ``True``, disables flushing to disk after copy (for
                         better performance).
- ``compress`` (optional): If ``True``, compresses the copy using gzip (same
                           behavior as ``gzip`` action)

**sendfile**: Uses the ``sendfile(2)`` system call to efficiently copy a file to
a destination path.

Parameters:
- ``targetpath`` (mandatory): Path to the copy target.
- ``nosync`` (optional): If ``True``, disables sync after copy.

**gzip**: Copies and compresses a file using gzip.

Parameters:
- ``nosync`` (optional): If ``True``, disables sync after writing.

Example:

.. code:: python

   declare_policy(
       name = "copy_large_recent_files",
       target = (Size > "10MB") & (Last_Access < "7d"),
       action = common.copy,
       parameters = {
           "targetpath": "/mnt/archive/",
           "nosync": True
       },
       trigger = Periodic("hourly")
   )

This policy copies all files larger than 10MB and accessed within the last 7 days
to the ``/mnt/archive/`` directory every hour. The ``nosync`` parameter improves
performance by avoiding a disk flush after each copy operation.

Lustre HSM Actions
------------------

When Lustre is used with Hierarchical Storage Management (HSM), RobinHood
provides additional built-in actions for managing data movement between the
filesystem and a backend archive.

**lhsm.archive**: Archives a file to the backend using the Lustre HSM copytool.

Parameters:
- ``archive_id`` (optional): Specifies which archive to target.
- Other key-value parameters (optional): Passed to the copytool as a
  comma-separated string (e.g., ``k1=v1,k2=v2``), like in
  ``lfs hsm_archive --data``.

**lhsm.release**: Releases the data of a file from the Lustre filesystem. The
file must be synchronized. This frees space on Lustre without deleting metadata.

**lhsm.remove**: Deletes the file from the backend archive. Typically used when
a file has already been removed from Lustre.

Modularity and Extensibility
----------------------------
