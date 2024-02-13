#!/bin/bash

function update_directory_expiration_date
{
    local database"$1"
    local directory_ID="$2"
    local expiration_date="$3"

    echo "--- !inode_xattr
\"id\": !!binary $directory_ID
\"xattrs\":
  \"user.ccc_expiration_date\": !int64 $expiration_date
..." | rbh-fsevents - "rbh:mongo:$database"
}

function check_expiration_date
{
    local database"$1"
    local directory="$2"
    local directory_ID="$3"
    local expires="$4"
    local expiration_date="$5"

    local max_time=$(rbh-find "rbh:mongo:${database}#$directory" \
                        -printf "%A\n%T\n" | sort | tail -n 1)

    echo "Directory '$directory' expiration date is set to '$expiration_date'"
    echo "The last accessed file in it was accessed at '$max_time'"

    local current="$(date +%s)"

    if (( max_time + expires > current)); then
        local new_expiration_date=$((max_time + expires + 1))

        echo "Expiration of the directory should occur '$expires' seconds after it's last usage"
        echo "Changing the expiration date of '$directory' to '$new_expiration_date'"
        update_directory_expiration_date "$database" "$directory_ID" \
                                         "$new_expiration_date"
    else
        echo "Directory '$directory' is truly expired"
    fi
}

function run_retention_updater
{
    local database="$1"

    local expired_directories="$(rbh-lfind rbh:mongo:"$database" -type d \
                                    -expired -printf "%p|%e|%E|%I\n")"

    if [ -z "${expired_directories}" ]; then
        echo "No directory has expired yet"
        exit 0
    fi

    while IFS= read -r line; do
        local directory=("$(echo $line | cut -d'|' -f1)")
        local expires=("$(echo $line | cut -d'|' -f2)")
        local expiration_date=("$(echo $line | cut -d'|' -f3)")
        local directory_ID=("$(echo $line | cut -d'|' -f4)")

        check_expiration_date "$database" "$directory" "$directory_ID" \
                              "$expires" "$expiration_date"
    done <<< "$expired_directories"
}

if [ $# -eq 0 ]; then
    echo "Please specify a database to update"
    exit 1
elif [ $# -gt 1 ]; then
    echo "Please only specify one argument"
    exit 1
fi

database="$1"

run_retention_updater "$database"
