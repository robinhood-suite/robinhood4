.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

------------------------------------------------------------
RobinHood 4 Policy Engine – Design Document
------------------------------------------------------------

Overview
========
The RobinHood 4 Policy Engine is designed to automate the execution of specific
actions on storage elements based on rules defined by administrators.
This document describes the functionality, configuration syntax, and high-level
implementation of the policy engine.
The engine is executed via the ``rbh-policy`` command, which takes the name of
the policies to execute as arguments.
A daemon will be added later to manage the execution of policies automatically.
We will have two execution modes:
- The first is to execute a policy explicitly by specifying its name via the
  ``rbh-policy`` command. This allows administrators to force the execution of
  a policy at any time, based on the trigger.
- The second is to let the daemon automatically execute policies based on
  defined triggers.
In both cases, policies and their behavior are defined entirely in the
configuration file.

Configuration is organized per filesystem and stored in a standard location
(e.g., ``/etc/robinhood4/``). For a given filesystem (e.g., fs1), the policy
engine automatically locates and loads its configuration by searching for a
Python file named after the filesystem: /etc/robinhood4/fs1.py.

Alternatively, administrators may explicitly provide a full path to a
configuration file via the command line. This allows for flexible overrides or
testing without relying on the default location.

If administrators wish to split configuration across multiple files, they are
responsible for managing imports within their main configuration file. The
engine does not recursively load directories or resolve dependencies across
files. Modularization must be handled explicitly by the user.

To reuse or share logic across multiple filesystems, common modules (e.g.,
``common.py``) can be created and imported manually into each configuration file
that requires them.

Python has been chosen as the configuration language because it provides the
flexibility of a full-fledged programming language. This allows administrators
to easily define and understand policies, while also enabling them to implement
advanced non-default behaviors directly within the configuration file, if
desired. Unlike traditional configuration formats such as YAML or JSON, Python
eliminates the need for additional scripting (e.g., Bash scripts) by offering
all the tools and capabilities needed within a single environment.
Additionally, having the entire system, from the configuration to the policy
engine and the daemon built in Python ensures seamless integration and provides
a consistent and unified development environment.

While this document uses files as the primary example for clarity, the policy
engine operates on all storage elements visible in the filesystem. This includes
regular files, directories, symbolic links, and other types of storage, such as
object stores, all of which can be targeted by policies.

Goals
=====
* Provide administrators with a simple and clear configuration interface to
  define and manage policies.
* Enable file grouping via reusable file classes based on criteria.
* Automate the execution of actions on files based on administrator-defined
  rules.
* Allow administrators to incorporate advanced behaviors in their
  configuration for customized functionality.
* Support orchestration and automation of policy execution with a daemon,
  ensuring streamlined and efficient management.
* Ensure scalability and high performance, even on large-scale filesystems,
  by minimizing overhead and optimizing trigger evaluation and file selection.
