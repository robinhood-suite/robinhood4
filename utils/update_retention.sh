#!/bin/bash

function update_directory_expiration_date
{
    local URI="$1"
    local directory_ID="$2"
    local expiration_date="$3"

    echo "--- !inode_xattr
\"id\": !!binary $directory_ID
\"xattrs\":
  \"user.ccc_expiration_date\": !int64 $expiration_date
..." | rbh-fsevents - "$URI"
}

function check_expiration_date
{
    local URI="$1"
    local directory="$2"
    local directory_ID="$3"
    local expires="$4"
    local expiration_date="$5"

    local max_time=$(rbh-find "$URI#$directory" \
                        -printf "%A\n%T\n" | sort | tail -n 1)

    echo "Directory '$directory' expiration date is set to '$expiration_date'"
    echo "The last accessed file in it was accessed at '$max_time'"

    local current="$(date +%s)"

    if (( max_time + expires > current)); then
        local new_expiration_date=$((max_time + expires + 1))

        echo "Expiration of the directory should occur '$expires' seconds after it's last usage"
        echo "Changing the expiration date of '$directory' to '$new_expiration_date'"
        update_directory_expiration_date "$URI" "$directory_ID" \
                                         "$new_expiration_date"
    else
        echo "Directory '$directory' has expired"
    fi
}

function run_retention_updater
{
    local URI="$1"

    rbh-lfind $URI -type d -expired -expired -printf "%p|%e|%E|%I\n" |
        while IFS='|' read -r dir expires expiration_date ID; do

        check_expiration_date "$URI" "$dir" "$ID" "$expires" "$expiration_date"
    done
}

if [ $# -eq 0 ]; then
    echo "Please specify a URI to update"
    exit 1
elif [ $# -gt 1 ]; then
    echo "Please only specify one argument"
    exit 1
fi

URI="$1"

run_retention_updater "$URI"
