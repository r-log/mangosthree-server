#!/bin/bash

exclude=(
    "SD3"
    "realmd"
)

exclude_dirs=""
for dir in "${exclude[@]}"; do
    exclude_dirs+="--exclude-dir=$dir "
done

set -e

echo "Starting Codestyling Script:"
echo

declare -A singleLineRegexChecks=(
    ["[[:blank:]]$"]="Remove whitespace at the end of the lines above"
    ["	"]="Replace tabs with 4 spaces in the lines above"
)

for check in "${!singleLineRegexChecks[@]}"; do
    echo "  Checking RegEx: '${check}'"

    set +e
    grep -P -r -I -n ${exclude_dirs} "${check}" src
    status=$?
    set -e

    if [ ${status} -eq 0 ]; then
        echo
        echo "${singleLineRegexChecks[$check]}"
        exit 1
    elif [ ${status} -ne 1 ]; then
        echo
        echo "grep exited ${status} for '${check}': the check did not run."
        exit 1
    fi
done

echo
echo "Awesome! No issues..."
