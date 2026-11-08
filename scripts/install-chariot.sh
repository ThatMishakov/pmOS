#!/bin/sh
set -eu

# This release corresponds to the tested v3 commit
# 8a64624d4279685ed2add4889476424291219a50.
CHARIOT_RELEASE=2026-11-01
CHARIOT_SHA256=4e052a07d50b103f1af917a8236849ce618c7eb746b451d7909409fb7303e0d5

case "$(uname -s)/$(uname -m)" in
    Linux/x86_64) ;;
    *)
        echo "The pinned Chariot release provides a Linux x86-64 binary only." >&2
        echo "Set CHARIOT=/absolute/path/to/a/compatible/chariot when invoking Make." >&2
        exit 1
        ;;
esac

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHARIOT_BIN_DIR="$PROJECT_DIR/.chariot-tools/bin"
mkdir -p "$CHARIOT_BIN_DIR"
CHARIOT_DOWNLOAD=$(mktemp "$CHARIOT_BIN_DIR/.chariot.XXXXXX")
trap 'rm -f "$CHARIOT_DOWNLOAD"' 0
trap 'exit 1' HUP INT TERM

curl --fail --location --show-error \
    "https://github.com/chariot-build/chariot/releases/download/$CHARIOT_RELEASE/chariot-linux-x86_64" \
    --output "$CHARIOT_DOWNLOAD"
printf '%s  %s\n' "$CHARIOT_SHA256" "$CHARIOT_DOWNLOAD" | sha256sum --check --status
chmod 755 "$CHARIOT_DOWNLOAD"
mv -f "$CHARIOT_DOWNLOAD" "$CHARIOT_BIN_DIR/chariot"
touch "$CHARIOT_BIN_DIR/chariot"
