"""Example RobinHood4 Policy Engine configuration template.

Copy and adapt this file, then run policies with:
    rbh-policy run <fs_name_or_abs_path> <policy1[,policy2,...]>
"""

from rbhpolicy.config.core import *


# Global policy engine configuration.
#
# filesystem: metadata source to evaluate (files/directories to inspect)
# database: backend that stores indexed metadata used by policies
# evaluation_interval: frequency used by the engine for policy evaluations
config(
    filesystem="rbh:posix:/mnt/data",
    database="rbh:mongo:test",
    evaluation_interval="5m",
)


# Reusable targets (fileclasses).
#
# A fileclass is a named filter you can combine with others to avoid
# duplicating complex target expressions in many policies.
#
# Common filter entities include: Path, Name, IName, Type, Size, User,
# LastAccess, LastModification, CreationDate...
#
# Logical operators:
#   &   AND
#   |   OR
#   ~   NOT
large_files = declare_fileclass(
    name="large_files",
    target=(Type == "f") & (Size >= "10GB"),
)

old_files = declare_fileclass(
    name="old_files",
    target=(Type == "f") & (LastAccess > "180d"),
)

logs = declare_fileclass(
    name="logs",
    target=IName == "*.log",
)


# Custom Python action.
#
# Signature must be: function(path, **kwargs)
# - path is the absolute path of the matched entry.
# - kwargs receives values from policy parameters.
# Return 0 for success, non-zero for failure.
def archive_with_metadata(path, retention_days=None, notify=False):
    """Example custom action callable used by a policy."""
    print(
        f"Archive {path} (retention_days={retention_days}, notify={notify})"
    )
    return 0


# Policy 1: safe audit policy.
#
# Uses action.log to validate that the target selection is correct before
# enabling destructive actions.
declare_policy(
    name="audit_large_old_files",
    target=large_files & old_files,
    action=action.log,
    trigger=Periodic("daily"),
)


# Policy 2: deletion policy with guard rails.
#
# - Target only old files that are not logs.
# - Remove empty parent directories after file deletion when possible.
# - Exclude files owned by root with a rule.
#
# Rule behavior: first matching rule wins.
declare_policy(
    name="cleanup_old_non_logs",
    target=old_files & ~logs,
    action=action.delete,
    trigger=Periodic("weekly"),
    parameters={
        "remove_empty_parent": True,
        "remove_parents_below": "/archive",
    },
    rules=[
        Rule(
            name="skip_root_files",
            condition=User == "root",
            action=None,
        ),
    ],
)


# Policy 3: custom business action.
#
# This shows how to call your own Python function and pass action-specific
# parameters through the policy parameters dictionary.
declare_policy(
    name="custom_archive",
    target=(Type == "f") & (Size >= "1GB") & (LastAccess > "90d"),
    action=archive_with_metadata,
    trigger=Periodic("7d"),
    parameters={
        "retention_days": 365,
        "notify": True,
    },
)
