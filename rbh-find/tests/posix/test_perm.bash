#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
# alternatives

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

dirs=(`mktemp --dir; mktemp --dir; mktemp --dir; mktemp --dir`)
# mktemp creates directory with permission 700 which conflicts with some tests
chmod g=u ${dirs[@]}

function error
{
    echo "$*"
    exit 1
}

function test_octal
{
    local dir="${dirs[0]}"

    local perms=(001 002 003 004 005 006 007
                 010 020 030 040 050 060 070
                 100 200 300 400 500 600 700
                 111 222 333 444 555 666 777)
    local perm_minus=(8 8 4 8 4 4 2
                      8 8 4 8 4 4 2
                      8 8 4 8 4 4 2
                      4 4 2 4 2 2 1)
    local perm_slash=(8  8 12  8 12 12 14
                      8  8 12  8 12 12 14
                      8  8 12  8 12 12 14
                      16 16 24 16 24 24 28)

    for perm in ${perms[@]}; do
        touch "$dir/file.$perm"
        chmod "$perm" "$dir/file.$perm"
    done

    rbh_sync "rbh:posix:$dir" "rbh:$db:$testdb"

    for ((i = 0; i < ${#perms[@]}; i++)); do
        local num=$(rbh_find "rbh:$db:$testdb" -perm "${perms[i]}" | wc -l)

        (( "$num" == 1 )) ||
            error "rbh-find -perm ${perms[i]}: $num != 1"

        num=$(rbh_find "rbh:$db:$testdb" -perm "-${perms[i]}" -type f | wc -l)
        (( "$num" == "${perm_minus[i]}" )) ||
            error "rbh-find -perm -${perms[i]}: $num != ${perm_minus[i]}"

        num=$(rbh_find "rbh:$db:$testdb" -perm "/${perms[i]}" -type f | wc -l)
        (( "$num" == "${perm_slash[i]}" )) ||
            error "rbh-find -perm /${perms[i]}: $num != ${perm_slash[i]}"
    done
}

function test_symbolic
{
    local dir="${dirs[1]}"
    local perms=(1000 2000 4000 333 444 777 644 111 110 100 004 001)
    local symbolic=(a+t /+t +t o+t ugo+t g+s u+s o+sr
                    o=r,ug+o,u+w o=r,ug+o,u+w u=r,a+u,u+w
                    g=r,ugo=g,u+w u+x,+X
                    u+x,u+X u+x,g+X o+r,+X
                    u+x,go+X +wx +rwx
                    u=r+w-w+wx,g+u,o+wrx
                    a=r,-u,o+x =644 a=r,a=w,a=x
                    =640,=777 g=w,a=r)

    for perm in "${perms[@]}"; do
        touch "$dir/file.$perm"
        chmod "$perm" "$dir/file.$perm"
    done

    rbh_sync "rbh:posix:$dir" "rbh:$db:$testdb"

    for (( i = 0; i < ${#symbolic[@]}; i++ )); do
        local num=$(rbh_find "rbh:$db:$testdb" -perm "${symbolic[i]}" | wc -l)

        (( $num == 1 )) ||
            error "rbh-find rbh:$db:$testdb -perm ${symbolic[i]}: $num != 1"
    done
}

function test_null_perm
{
    local dir="${dirs[2]}"
    local symbolic=(u+t g+t o+s +s u+ g+ o+ a+ ugo+
                    u- g- o- a- ugo- u= g= o= a=
                    ugo= +X u+X + = u+r,u-r u=x,-100)

    touch "$dir/file.000"
    chmod 000 "$dir/file.000"

    rbh_sync "rbh:posix:$dir" "rbh:$db:$testdb"

    for (( i = 0; i < ${#symbolic[@]}; i++ )); do
        local num=$(rbh_find "rbh:$db:$testdb" -perm "${symbolic[i]}" | wc -l)

        (( $num == 1 )) ||
            error "rbh-find rbh:$db:$testdb -perm ${symbolic[i]}: $num != 1"
    done
}

function test_error_perm
{
    local dir="${dirs[3]}"
    local tests=(17777 787 789 abcd ug=uu ug=a
                ug=gu uo=ou urw u+xg+x a=r,u+x,
                / - a u=r,u u,u=r)

    rbh_sync "rbh:posix:$dir" "rbh:$db:$testdb"

    for err in "${tests[@]}"; do
        if rbh_find "rbh:$db:$testdb" -perm "$err"; then
            error "rbh-find -perm $err: parsing should have failed"
        fi
    done
}

function cleanup
{
    rm -rf ${dirs[@]}
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_octal test_null_perm test_symbolic test_error_perm)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
