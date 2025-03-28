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

- ``Path``: Full file path (supports regular expressions).
  Example: ``Path == "/tmp/scratch/cout*/*/"``
  Matches files located exactly in the specified path.

- ``Name``: Case-sensitive file name.
  Example: ``Name == "report.txt"``
  Matches files with the exact name "report.txt".

- ``IName``: Case-insensitive file name.
  Example: ``IName == "report.txt"``
  Matches "report.txt", "REPORT.TXT", "Report.TxT", etc.

- ``Type``: File type, either ``"file"``, ``"dir"`` or ``"symlink"``.
  Example: ``Type == "file"``
  Selects only regular files.

- ``Owner``: Username of the file owner.
  Example: ``Owner == "admin"``
  Matches files owned by the "admin" user.

- ``UID``: Numeric user ID of the file owner.
  Example: ``UID == 1001``
  Matches files owned by the user with UID 1001.

- ``Group``: Group name of the file owner.
  Example: ``Group == "developers"``
  Matches files where the owning group is "developers".

- ``Size``: File size with units ``"KB"``, ``"MB"``, ``"GB"``, ``"TB"``.
  Example: ``Size >= "10GB"``
  Selects files that are at least 10 GB in size.

- ``DirCount``: Total number of elements in a directory.
  Example: ``Dircount > 100``
  Selects directories containing more than 100 elements.

- ``LastAccess``: Last access time, supporting relative values
  (``d`` = days, ``h`` = hours, ``m`` = minutes).
  Example: ``LastAccess > "30d"``
  Selects files not accessed in the last 30 days.

- ``LastModification``: Last modification time.
  Example: ``LastModification > "90d"``
  Selects files that have not been modified in the last 90 days.

- ``LastChange``: Last metadata change time.
  Example: ``LastChange > "60d"``
  Selects files whose metadata (permissions, owner, etc.)
  hasn't changed in the last 60 days.

- ``OstPool``: OST pool where the file is stored (for Lustre).
  Example: ``OstPool == "fast_pool"``
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
- ``in``  (value is in a list)
Exemple:   ``Owner in ["user1", "user2"]``
To negate: ``~(Owner in ["user1", "user2"])``

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
Select directories with more than 500 subdirectories OR owned by "admin":

.. code:: python

    (Type == "dir") & ((Dircount > 500) | (Owner == "admin"))

**Example 3:**
Select entries that do not belong to the 'recent_files' fileclass, do not exceed
10MB in size, and have been accessed in the last 180 days. This condition
therefore targets files less than 10MB in size and which have a last access date
between 30 and 180 days ago.

.. code:: python

    declare_fileclass(name = "recent_file", condition = (LastAccess <  "30d"))

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
