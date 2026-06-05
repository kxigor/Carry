#!/usr/bin/env bash

set -euo pipefail

usage() {
    echo "Usage: $0 --format-exec <clang_format_executable> --config <config_file> --files <file_list>"
    exit 1
}

FORMAT_CMD=""
CONFIG_FILE=""
FILES=()

while [[ $# -gt 0 ]]; do
    case $1 in 
        --format-exec)
            FORMAT_CMD="$2"
            shift 2
            ;;
        --config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        --files)
            shift
            FILES=("$@")
            break
            ;;
        *)
            echo "Unknown argument: $1"
            usage
            ;;
    esac
done

if [[ -z "$FORMAT_CMD" ]]; then
    echo "Error: Format command not specified."
    usage
fi

if ! command -v "$FORMAT_CMD" &> /dev/null; then
    echo "Error: $FORMAT_CMD is not installed or not in PATH."
    exit 1
fi

if [[ -z "$CONFIG_FILE" ]]; then
    echo "Error: Config file not specified."
    usage
fi

if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "Error: Config file '$CONFIG_FILE' not found."
    exit 1
fi

FAILED=0
echo "Checking format with config: $CONFIG_FILE"

for FILE in "${FILES[@]}"; do
    if [[ ! -f "$FILE" ]]; then
        echo "Warning: File $FILE not found, skipping."
        continue
    fi

    if ! diff -u "$FILE" <($FORMAT_CMD -style=file:"$CONFIG_FILE" "$FILE"); then
        echo "Format violation: $FILE"
        FAILED=1
    fi
done

if [[ $FAILED -ne 0 ]]; then
    echo "FAILED: Some files are not formatted correctly."
    exit 1
else
    echo "SUCCESS: All files formatted correctly."
    exit 0
fi