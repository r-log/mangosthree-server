#!/bin/bash

# SD3 and realmd are vendored upstream code in upstream's style. They are ours to edit now,
# but they have not been restyled, so the style check skips them until that happens.
exclude=(
    # dirs
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

    # grep's three exit codes are three different answers and only two of them are
    # "the check ran": 0 found something, 1 found nothing, anything else FAILED to
    # look -- an unreadable file, a locale it cannot use for -P. Written as
    # `if grep ...; then` an error reads the same as a clean pass, and the gate
    # reports success without having checked a single line.
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
