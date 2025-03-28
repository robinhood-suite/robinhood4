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
  ``RBH_POLICY_EXTRA_CONFIG`` would instruct the engine to load an extra file or
  directory alongside the main one.

These improvements are under consideration and aim to make policy management
more flexible and maintainable, especially in large or multi-filesystem
environments.

CLI extensions
--------------

The following feature could improve the usability and observability of
fileclasses in Robinhood:

- Fileclass-based reporting and inspection:

  Additional CLI commands could be introduced to inspect which files match a
  given fileclass, or to explore fileclasses associated with specific files.
  Example commands:
  - ``rbh-policy fileclass entries <fileclass>``:
    Lists all entries currently matching the given fileclass.

  - ``rbh-policy fileclass entries --fs fs1 <class>``:
    Restricts the search to a specific filesystem.

  - ``rbh-policy fileclass describe <path>``:
    Displays all fileclasses to which the given file belongs.

Note: These features are closely related to reporting and classification
      inspection. They may be more appropriate for implementation in tools like
      ``rbh-report`` rather than in the policy execution engine itself maybe.
