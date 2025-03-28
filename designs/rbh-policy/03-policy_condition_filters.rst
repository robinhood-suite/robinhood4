.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

Policy Condition Filters
========================
The RobinHood 4 Policy Engine allows administrators to define file selection
criteria using specific filters in the policy configuration.
These filters can be combined using logical operators (``&``, ``|``, ``~``)
and standard comparison operators.

Supported Filters
-----------------
Filters must be used exactly as defined below. Any unsupported filter or
incorrect syntax will result in a policy configuration error.
We chose PascalCase keywords to avoid conflicts with Python's reserved words
(e.g., type).

- ``Path``: Full file path.
  Example: ``Path == "/tmp/scratch/cout*/*/"``
  Matches files located in the specified path.
  This behaves similarly to the ``-path`` option in ``rbh-find``.

- ``Name``: Case-sensitive file name.
  Example: ``Name == "report.txt"``
  Matches files with the exact name "report.txt".

- ``IName``: Case-insensitive file name.
  Example: ``IName == "report.txt"``
  Matches "report.txt", "REPORT.TXT", "Report.TxT", etc.

- ``Type``: File type, either ``"file"``, ``"dir"`` or ``"symlink"`` or any
            other storage element visible in the filesystem.
  Example: ``Type == "file"``
  Selects only regular files.

- ``User``: Username(s) of the file owner(s).
  Accepts a single username or a ClusterShell-compatible list.
  Examples:
    - ``User == "admin"``
    - ``User in ["admin","user[01-03]","test42"]``
  Matches files owned by any of the specified users.

- ``UID``: Numeric user ID of the file owner.
  Example: ``UID == 1001``
  Matches files owned by the user with UID 1001.

- ``Group``: Group name(s) of the file owner(s).
  Accepts a single group name or a ClusterShell-compatible list.
  Examples:
    - ``Group == "developers"``
    - ``Group in ["dev[1-4]","ops42"]``
  Matches files where the owning group matches any of the listed names.

- ``GID``: Numeric group ID of the file owner.
  Example: ``GID == 1010``
  Matches files owned by the group with GID 1010.

- ``Size``: File size with units ``"KB"``, ``"MB"``, ``"GB"``, ``"TB"``, ``PB``.
  Example: ``Size >= "10GB"``
  Selects files that are at least 10 GB in size.

- ``DirCount``: Total number of elements in a directory.
  Example: ``Dircount > 100``
  Selects directories containing more than 100 elements.

- ``LastAccess``: Last access time (corresponds to Unix ``atime``).
  Supports both relative durations (``d`` = days, ``h`` = hours,
  ``m`` = minutes) and absolute timestamps (e.g., ``"2025-05-12"``,
  ``"2025-05-12 03:00"``).
  Examples:
    - ``LastAccess > "30d"``
    - ``LastAccess < "2025-06-01 03:00"``
  Selects files that have not been accessed within the specified time frame.

- ``LastModification``: Last modification time (corresponds to Unix ``mtime``).
  Supports both relative durations and absolute timestamps.
  Examples:
    - ``LastModification > "90d"``
    - ``LastModification < "2025-01-01"``
  Selects files whose content has not been modified within the specified time
  frame.

- ``LastChange``: Last metadata change time (corresponds to Unix ``ctime``).
  Supports both relative durations and absolute timestamps.
  Examples:
    - ``LastChange > "60d"``
    - ``LastChange < "2025-05-12"``
  Selects files whose metadata (permissions, ownership, etc.) has not changed
  within the specified time frame.

- ``CreationDate``: File creation time (corresponds to Unix ``btime``).
  Supports both relative durations and absolute timestamps.
  Examples:
    - ``CreationDate > "180d"``
    - ``CreationDate < "2024-12-31"``
  Selects files based on when they were originally created.

- ``Pool``: OST pool where the file is stored (for Lustre).
  Example: ``Pool == "fast_pool"``
  Selects files stored in the OST pool named "fast_pool".

Supported Operators
-------------------
Comparison operators:
- ``==``  (equal to)
- ``!=``  (not equal to)
- ``>``   (greater than)
- ``>=``  (greater than or equal to)
- ``<``   (less than)
- ``<=``  (less than or equal to)

Logical operators:
- ``&``   (logical AND)
- ``|``   (logical OR)
- ``~``   (logical NOT)

Membership Operators:
- IN  (value is in a list)
Exemple:   ``User.IN("user1", "user2")``
To negate: ``~(User.IN("user1", "user2"))``

Storage and Time Units
----------------------
Storage units:
- ``B``   (bytes)
- ``KB``  (kilobytes)
- ``MB``  (megabytes)
- ``GB``  (gigabytes)
- ``TB``  (terabytes)
- ``PB``  (petabytes)
- ``EB``  (exabytes)
- ``%``   (percentage)

Large quantity units:
- ``k``   (thousands)
- ``M``   (millions)
- ``Bn``  (billions)
- ``Tn``  (trillion)
- ``%``   (percentage)

Time units:
- ``s``   (seconds)
- ``m``   (minutes)
- ``h``   (hours)
- ``d``   (days)
- ``w``   (weeks)
- ``mo``  (months)
- ``y``   (years)

Examples of Complex Conditions
------------------------------
Conditions can be combined to create advanced filtering rules.

**Example 1:**
Select files larger than 3GB, that are regular files, and haven't been accessed
in 180 days:

.. code:: python

    (Size >= "3GB") & (Type == "file") & (LastAccess > "180d")

**Example 2:**
Select directories containing more than 500 items OR owned by "admin"

.. code:: python

    (Type == "dir") & ((Dircount > 500) | (User == "admin"))

**Example 3:**
Select entries that do not belong to the 'recent_files' fileclass, do not exceed
10MB in size, and have been accessed in the last 180 days. This condition
therefore targets files less than 10MB in size and which have a last access date
between 30 and 180 days ago.

.. code:: python

    declare_fileclass(name = "recent_file", target = (LastAccess <  "30d"))

    ~recent_files & (Size < "10MB") & (LastAccess < "180d")

Note on Logical Operators
-------------------------
If administrators prefer a more readable syntax using ``and``, ``or``,
and ``not``, it is important to note that Python does not allow overloading
these operators. To work around this limitation, an alternative approach
could be to write conditions as strings and this string can then be parsed and
evaluated by the policy engine. However, for the current implementation,
the syntax using ``&``, ``|``, and ``~`` should be used to ensure correct
behavior.
