#!/usr/bin/env bash

# This file is part of rbh-fsevents.
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

################################################################################
#                                  UTILITIES                                   #
################################################################################

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

__rbh_fsevents=$(PATH="$PWD:$PATH" which rbh-fsevents)
rbh_fsevents()
{
    "$__rbh_fsevents" "$@"
}

__mongo=$(which mongosh || which mongo)
mongo()
{
    "$__mongo" --quiet "$@"
}


################################################################################
#                                DATABASE UTILS                                #
################################################################################


invoke_rbh-fsevents()
{
    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"
}

find_attribute()
{
    old_IFS=$IFS
    IFS=','
    local output="$*"
    IFS=$old_IFS
    local res=$(mongo $testdb --eval "db.entries.count({$output})")
    [[ "$res" == "1" ]] && return 0 ||
        error "No entry found with filter '$output'"
}

################################################################################
#                                 LUSTRE UTILS                                 #
################################################################################

hsm_archive_file()
{
    local file="$1"

    lfs hsm_archive "$file"

    while ! lfs hsm_state "$file" | grep "archive_id:"; do
        sleep 0.5
    done
}

hsm_release_file()
{
    local file="$1"

    lfs hsm_release "$file"

    while ! lfs hsm_state "$file" | grep "released"; do
        sleep 0.5
    done
}

hsm_restore_file()
{
    local file="$1"

    lfs hsm_restore "$file"

    while lfs hsm_state "$file" | grep "released"; do
        sleep 0.5
    done
}

hsm_remove_file()
{
    local file="$1"

    lfs hsm_remove "$file"

    while lfs hsm_state "$file" | grep "archived"; do
        sleep 0.5
    done
}

get_hsm_state()
{
    # retrieve the hsm status which is in-between parentheses
    local state=$(lfs hsm_state "$1" | cut -d '(' -f2 | cut -d ')' -f1)
    # the hsm status is written in hexa, so remove the first two characters (0x)
    state=$(echo "${state:2}")
    # bc only understands uppercase hexadecimals, but hsm state only returns
    # lowercase, so we use some bashism to uppercase the whole string, and then
    # convert the hexa to decimal
    echo "ibase=16; ${state^^}" | bc
}

get_comp_values()
{
    local entry="$1"
    local comp_count=$2
    local arr=()

    for i in $(seq 1 $comp_count); do
        local val=$(lfs getstripe $3 --component-id=$i $entry)
        val="${val//EOF/-1}"
        arr+=($val)
    done

    echo "${arr[@]}"
}

get_begin()
{
    local begins=$(get_comp_values $1 $2 --component-start)

    echo "$(build_long_array "${begins[@]}")"
}

get_cflags()
{
    local comp_flags=$(get_comp_values $1 $2 --component-flags)

    for i in $(seq 0 $2); do
        comp_flags[$i]="${comp_flags[$i]//init/16}"
    done

    echo "$(build_long_array "${comp_flags[@]}")"
}

get_end()
{
    local ends=$(get_comp_values $1 $2 --component-end)

    echo "$(build_long_array "${ends[@]}")"
}

get_mirror_id()
{
    local entry="$1"

    local mids=$(lfs getstripe -v "$entry" | grep "lcme_mirror_id" | \
                 cut -d ':' -f2 | xargs)
    echo "$(build_long_array "${mids[@]}")"
}

get_ost()
{
    local entry="$1"
    local comp_count=$2
    local mirror_count=$3
    local osts=()

    if [[ $comp_count == 0 ]]; then
        osts+=$(lfs getstripe -y "$entry" | grep "l_ost_idx" | \
                cut -d ':' -f2 | cut -d ',' -f1 | xargs)
        echo "$(build_long_array "${osts[@]}")"
        return
    fi

    local count
    local option

    # TODO: this does not manage cases where we have a mirrored file with one
    # mirror being on multiple components
    if [[ $mirror_count > 1 ]]; then
        count=$mirror_count
        option="--mirror-id"
    else
        count=$comp_count
        option="--component-id"
    fi

    for i in $(seq 1 $count); do
        local n_objects=$(lfs getstripe -y "$entry" $option=$i | \
                          grep "l_ost_idx" | wc -l)

        if [[ $n_objects == 0 ]]; then
            osts+=(" -1")
        else
            osts+=($(lfs getstripe -y "$entry" $option=$i | grep "l_ost_idx" | \
                     cut -d ':' -f2 | cut -d ',' -f1 | xargs))
        fi
    done

    echo "$(build_long_array "${osts[@]}")"
}

get_pattern()
{
    local entry="$1"

    if [ -d $entry ]; then
        local patterns=$(lfs getstripe -L "$entry" | xargs)
    else
        local patterns=$(lfs getstripe -v "$entry" | grep "lmm_pattern" | \
                         cut -d ':' -f2 | xargs)
    fi

    patterns="${patterns//raid0,overstriped/4}"
    patterns="${patterns//mdt/2}"
    patterns="${patterns//raid0/0}"

    echo "$(build_long_array "${patterns[@]}")"
}

get_pool()
{
    local entry="$1"
    local n_comp=$2
    local n_mirror=$3
    local pools=()

    if [[ $n_comp == 0 ]]; then
        pools+=("\"$(lfs getstripe --pool "$entry")\"")
    elif [[ $n_mirror > 1 ]]; then
        for i in $(seq 1 $n_mirror); do
            pools+=("\"$(lfs getstripe --mirror-id=$i --pool "$entry")\"")
        done
    else
        for i in $(seq 1 $n_comp); do
            pools+=("\"$(lfs getstripe --component-id=$i --pool "$entry")\"")
        done
    fi

    echo "$(build_string_array "${pools[@]}")"
}

get_stripe_count()
{
    local entry="$1"
    local n_comp=$2
    local n_mirror=$3
    local stripes=()

    if [[ $n_comp == 0 ]]; then
        stripes+=("$(lfs getstripe -c "$entry")")
    elif [[ $n_mirror > 1 ]]; then
        for i in $(seq 1 $n_mirror); do
            stripes+=("$(lfs getstripe --mirror-id=$i -c "$entry")")
        done
    else
        for i in $(seq 1 $n_comp); do
            stripes+=("$(lfs getstripe --component-id=$i -c "$entry")")
        done
    fi

    echo "$(build_long_array "${stripes[@]}")"
}

get_stripe_size()
{
    local entry="$1"
    local n_comp=$2
    local n_mirror=$3
    local stripes=()

    if [[ $n_comp == 0 ]]; then
        stripes+=("$(lfs getstripe --stripe-size "$entry")")
    elif [[ $n_mirror > 1 ]]; then
        for i in $(seq 1 $n_mirror); do
            stripes+=("$(lfs getstripe --mirror-id=$i --stripe-size "$entry")")
        done
    else
        for i in $(seq 1 $n_comp); do
            stripes+=("$(lfs getstripe --component-id=$i --stripe-size "$entry")")
        done
    fi

    echo "$(build_long_array "${stripes[@]}")"
}

get_flags()
{
    local entry="$1"

    local flags=$(lfs getstripe -v "$entry" | grep "lcm_flags" | \
                  cut -d ':' -f2 | xargs)
    # Taken from linux/lustre/lustre_user.h, structure lov_comp_md_flags
    flags="${flags//ro/1}"
    flags="${flags//wp/2}"
    flags="${flags//sp/3}"

    echo "$flags"
}

verify_lustre()
{
    local entry="$1"
    local name="$(basename "$entry")"

    if [ -L $entry ] || [ -b $entry ] || [ -c $entry ] || [ -p $entry ]; then
        return 0
    fi

    # $n_comp is 0 if the entry is non-composite
    local n_comp=$(lfs getstripe --component-count "$entry")
    local gen=$(lfs getstripe -g "$entry" | head -n 1)
    local mdt=$(lfs getstripe -m "$entry" | xargs)
    local n_mirror=$(lfs getstripe -N "$entry" | xargs)
    local hash_type=$(lfs getdirstripe -H "$entry")
    local dir_data_layout=$(lfs getstripe -L "$entry" | xargs)

    find_attribute '"ns.name":"'$name'"'
    find_attribute '"ns.xattrs.path":"'$(mountless_path "$entry")'"'

    # If the entry has multiple components but isn't mirrored, check each
    # sub-values of the components
    if [[ $n_comp != 0 && $n_mirror == 1 ]]; then
        find_attribute '"xattrs.begin":'$(get_begin "$entry" $n_comp) \
                       '"ns.name":"'$name'"'
        find_attribute '"xattrs.comp_flags":'$(get_cflags "$entry" $n_comp) \
                       '"ns.name":"'$name'"'
        find_attribute '"xattrs.end":'$(get_end "$entry" $n_comp) \
                       '"ns.name":"'$name'"'
        find_attribute '"xattrs.flags":'$(get_flags "$entry") \
                       '"ns.name":"'$name'"'
    fi

    # If the entry has multiple components or is mirrored, check the flags
    if [[ $n_comp != 0 || $n_mirror > 1 ]]; then
        find_attribute '"xattrs.flags":'$(get_flags "$entry") \
                       '"ns.name":"'$name'"'
    # Otherwise if the entry isn't a directory or if its default pattern for
    # child entries isn't 0 (which means a default striping for child entries
    # for that directory is set), check the flag is 0
    elif [[ ! -d $entry || $dir_data_layout != "0" ]]; then
        find_attribute '"xattrs.flags":0' '"ns.name":"'$name'"'
    fi

    if [ -f $entry ]; then
        find_attribute '"xattrs.gen":'$gen '"ns.name":"'$name'"'
    fi

    # If the entry is mirrored, check the count and ids
    if [ ! -z $n_mirror ]; then
        find_attribute '"xattrs.mirror_count":'$n_mirror '"ns.name":"'$name'"'
        find_attribute '"xattrs.mirror_id":'$(get_mirror_id "$entry") \
                       '"ns.name":"'$name'"'
    fi

    # If the entry isn't a directory or if its default pattern for
    # child entries isn't 0 (which means a default striping for child entries
    # for that directory is set), check the other Lustre sub-values
    if [[ ! -d $entry || $dir_data_layout != "0" ]]; then
        find_attribute '"xattrs.mdt_index":'$mdt '"ns.name":"'$name'"'
        find_attribute '"xattrs.pool":'$(get_pool "$entry" $n_comp) \
                       '"ns.name":"'$name'"'
        find_attribute '"xattrs.stripe_count":'$(get_stripe_count "$entry" \
                                                 $n_comp $n_mirror) \
                       '"ns.name":"'$name'"'
        find_attribute '"xattrs.pattern":'$(get_pattern "$entry") \
                       '"ns.name":"'$name'"'
        find_attribute '"xattrs.stripe_size":'$(get_stripe_size "$entry" \
                                                $n_comp $n_mirror) \
                       '"ns.name":"'$name'"'
    fi

    # Directories's OSTs are not recorded, so skip this verification
    if [ ! -d $entry ]; then
        find_attribute '"xattrs.ost":'$(get_ost "$entry" $n_comp $n_mirror) \
                       '"ns.name":"'$name'"'
    fi
}

start_changelogs()
{
    userid=$(lctl --device "$LUSTRE_MDT" changelog_register | cut -d "'" -f2)
}

clear_changelogs()
{
    for i in $(seq 1 ${userid:2}); do
        lfs changelog_clear "$LUSTRE_MDT" "cl$i" 0
    done
}

################################################################################
#                                 POSIX UTILS                                  #
################################################################################

mountless_path()
{
    local entry="$1"

    # Get the fullpath of the file, but remove the starting mountpoint as it is
    # not kept in the database (but keep a '/')
    local path="$(realpath --no-symlinks $entry)"
    echo "/${path//$LUSTRE_DIR/}"
}

statx()
{
    local format="$1"
    local file="$2"

    local stat=$(stat -c="$format" "$file")
    if [ -z $3 ]; then
        echo "${stat:2}"
    else
        stat=${stat:2}
        echo "ibase=$3; ${stat^^}" | bc
    fi
}

verify_statx()
{
    local entry="$1"
    local name="$(basename "$entry")"
    local raw_mode="$(statx +%f "$entry" 16)"
    local type=$((raw_mode & 00170000))
    local mode=$((raw_mode & ~00170000))

    find_attribute '"ns.name":"'$name'"'
    find_attribute '"ns.xattrs.path":"'$(mountless_path "$entry")'"'
    find_attribute '"statx.atime.sec":NumberLong('$(statx +%X "$entry")')' \
                   '"ns.name":"'$name'"'
    find_attribute '"statx.atime.nsec":0' '"ns.name":"'$name'"'
    find_attribute '"statx.ctime.sec":NumberLong('$(statx +%Z "$entry")')' \
                   '"ns.name":"'$name'"'
    find_attribute '"statx.ctime.nsec":0' '"ns.name":"'$name'"'
    find_attribute '"statx.mtime.sec":NumberLong('$(statx +%Y "$entry")')' \
                   '"ns.name":"'$name'"'
    find_attribute '"statx.mtime.nsec":0' '"ns.name":"'$name'"'
    find_attribute '"statx.btime.nsec":0' '"ns.name":"'$name'"'
    find_attribute '"statx.blksize":'$(statx +%o "$entry") \
                   '"ns.name":"'$name'"'
    find_attribute '"statx.blocks":NumberLong('$(statx +%b "$entry")')' \
                   '"ns.name":"'$name'"'
    find_attribute '"statx.nlink":'$(statx +%h "$entry") \
                   '"ns.name":"'$name'"'
    find_attribute '"statx.ino":NumberLong("'$(statx +%i "$entry")'")' \
                   '"ns.name":"'$name'"'
    find_attribute '"statx.gid":'$(statx +%g "$entry") '"ns.name":"'$name'"'
    find_attribute '"statx.uid":'$(statx +%u "$entry") '"ns.name":"'$name'"'
    find_attribute '"statx.size":NumberLong('$(statx +%s "$entry")')' \
                   '"ns.name":"'$name'"'
    find_attribute '"statx.type":'$type '"ns.name":"'$name'"'
    find_attribute '"statx.mode":'$mode '"ns.name":"'$name'"'
}

################################################################################
#                                  TEST UTILS                                  #
################################################################################

join_arr() {
    local IFS="$1"

    shift
    echo "$*"
}

build_long_array()
{
    local output="$*"
    local arr=($output)

    for i in "${!arr[@]}"; do
        arr[$i]="NumberLong(${arr[$i]})"
    done

    echo "[$(join_arr ", " ${arr[@]})]"
}

build_string_array()
{
    local output="$*"
    local arr=($output)

    for i in "${!arr[@]}"; do
        if [ -z ${arr[$i]} ]; then
            arr[$i]='""'
        else
            arr[$i]="${arr[$i]}"
        fi
    done

    echo "[$(join_arr ", " ${arr[@]})]"
}

setup()
{
    # Create a test directory and `cd` into it
    testdir=$PWD/$SUITE-$test
    mkdir "$testdir"
    cd "$testdir"

    # Create test database's name
    testdb=$SUITE-$test
    clear_changelogs
}

teardown()
{
    mongo "$testdb" --eval "db.dropDatabase()" >/dev/null
    rm -rf "$testdir"
    clear_changelogs
}

error()
{
    echo "$*"
    exit 1
}

run_tests()
{
    local fail=0

    for test in "$@"; do
        (set -e; trap -- teardown EXIT; setup; "$test")
        if !(($?)); then
            echo "$test: ✔"
        else
            echo "$test: ✖"
            fail=1
        fi
    done

    return $fail
}
