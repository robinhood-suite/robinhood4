def custom_action(**kwargs):
    print("I'm a custom action", kwargs)

declare_fileclass(
    name="recent_files",
    condition=(Size >= 10) & (Last_Access > 180)
)

declare_fileclass(
    name="large_files",
    condition=(Size >= 50)
)

declare_policy(
    name="custom_action_policy",
    target=(Size > 100),
    action=custom_action,
)

declare_policy(
    name="archive_large_files",
    target=(Size > 100),
    action=archive_files,
    parameters={"compression": "gzip"},
    rules=[
        {
            "name": "very_large_files",
            "target": (Size > 500),
            "action": delete_files,
            "parameters": {}
        }
    ]
)

declare_policy(
    name="delete_old_files",
    target=(Last_Access > 365) & (Size < 5),
    action=delete_files,
    parameters={}
)
