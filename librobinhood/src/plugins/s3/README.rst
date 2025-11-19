.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

#########
S3 plugin
#########

The S3 plugin is a plugin that retrieves information from S3-compatible object
storage systems. It iterates through S3 buckets checking each entry and
enriching a new FSEntry with the object's metadata.

The iteration/enrichment part is used by rbh-sync_.
The predicates part is used by rbh-find_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync

Configuration
=============

The S3 plugin requires connection credentials and endpoint information, which
can be provided either through:

 * A configuration file
 * URI parameters

Configuration Parameters
------------------------

The following parameters can be specified in the configuration file under the
``s3`` section:

 * ``address`` : The S3 server address (e.g., ``127.0.0.1:9000``)
 * ``user``    : The S3 access key ID
 * ``password``: The S3 secret access key
 * ``region``  : The AWS region code (optional)
 * ``crt_path``: Path to the SSL certificate file for HTTPS connections
                 (optional)

.. code:: yaml

    s3:
        address: 127.0.0.1:9000
        user: minioadmin
        password: minioadmin
        region: us-east-1
        crt_path: /path/to/certificate.crt

URI Format
----------

Connection information can also be provided via URI:

.. code:: bash

    rbh-sync rbh:s3://username:password@host:port rbh:mongo:test

If connection details are not specified in the URI, the plugin will fall back to
the configuration file. The credentials and region information need to be always
specified in the configuration file as they cannot be included in the URI.

Enrichment
==========

Here are the information provided by the S3 plugin:

* For all objects:
  * statx information:
    * modify time:
      * seconds
      * nanoseconds
    * size
  * name
  * path (bucket/object format)
  * user metadata (custom metadata stored with S3 objects)
  * bucket

Filtering
=========

The S3 plugin defines predicates that will be used with rbh-find_. The
predicates define filters that will be usable by users to filter data retrieved
from a backend.

The following predicates are supported by the S3 plugin:
* -bucket
* -mmin
* -mtime
* -name
* -path
* -size

Only the predicates different from the POSIX plugin will be explained.

-bucket
-------

A predicate to filter entries based on the bucket they belong to.

.. code:: bash

    rbh-find rbh:mongo:test -bucket mybucket
    /mybucket/obj1

Printing
========

The S3 plugin defines directives that will be used with rbh-find_.
They corresponds to which data is printed when using the `-printf` and
`-fprintf` actions of rbh-find_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

The following directives are supported by the S3 plugin:

+-------------+--------------------------------------------+
|  Directive  | Information printed                        |
+=============+============================================+
|     '%b'    | The bucket the entry belongs to            |
+-------------+--------------------------------------------+
|     '%f'    | The name of the entry                      |
+-------------+--------------------------------------------+
|     '%I'    | The ID of the entry computed by RobinHood  |
+-------------+--------------------------------------------+
|     '%p'    | The path of the entry                      |
+-------------+--------------------------------------------+
|     '%s'    | The size of the entry                      |
+-------------+--------------------------------------------+
|     '%t'    | The last modification time of the entry in |
|             | ctime format                               |
+-------------+--------------------------------------------+
|     '%T'    | The last modification time of the entry    |
+-------------+--------------------------------------------+
