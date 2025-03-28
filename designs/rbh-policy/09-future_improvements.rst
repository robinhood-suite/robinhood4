.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

Future Improvements
===================

Modular loading
---------------

To improve modularity and configuration reuse, several evolutions are being
considered:

- If the configuration path points to a directory (e.g.,
  ``/etc/robinhood4.d/fs1/``), one idea would be to load all valid Python files
  within that directory in lexical order. This could allow administrators to
  split configuration elements across logical files:
  .. code:: text

     /etc/robinhood4.d/fs1/
     ├── fileclasses.py
     ├── archive.py
     ├── cleanup.py

  This approach would enable better separation of concerns (e.g.,
  separating fileclasses and policies), and help maintain large configurations
  more easily.

- The engine could also support files without a ``.py`` extension, provided
  their content is valid Python. This would make the loading logic more flexible
  and aligned with configuration file conventions.

- We are also considering allowing shared or default logic through a structure
  like:

  ``/etc/robinhood4.d/defaults/basic_fileclasses.py``

  which could be reused across multiple filesystem configurations.

- Finally, another possibility could be to support injection of additional
  configuration logic via an environment variable. For example, setting
  ``RBH_ADDITIONAL_CONFIG`` would instruct the engine to load an extra file or
  directory alongside the main one.

These improvements are under consideration and aim to make policy management
more flexible and maintainable, especially in large or multi-filesystem
environments.

CLI extensions
---------------------

To improve policy management and discoverability, additional CLI commands could
be introduced in future versions of ``rbh-policy``.

These would help administrators list and explore available policies and
fileclasses without manually inspecting the configuration files:

- ``rbh-policy policies list``:
  - Lists all known policies from all available configuration files.
  - Optionally shows in which filesystem or config file each policy is defined.

- ``rbh-policy fileclass list``:
  - Lists all fileclasses defined across configurations.
  - Useful for validating names or understanding reusable selection criteria.

- ``rbh-policy policies list --fs fs1``:
  - Filters the listing to a specific filesystem.

These commands are under consideration to improve configuration navigation and
enable interactive tooling in the future.
