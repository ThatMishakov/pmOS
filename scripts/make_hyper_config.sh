#!/bin/sh

write_service_entry() {
    FILE="$1"
    NAME="$2"
    cat <<EOF >> "$FILE"
module:
    path = "/${NAME}.elf"
    name = "${NAME}.elf;file"

module:
    path = "/${NAME}.yaml"
    name = "${NAME}.yaml;init-config"

EOF
}

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <output_file> <services>"
    exit 1
fi

FILE="$1"
SERVICES="$2"
cat <<EOF > "$FILE"
default-entry = "pmOS"

[pmOS]
protocol = "ultra"

binary:
    path = "/kernel"

page-table:
    levels = 3

EOF

for SERVICE in $SERVICES; do
    write_service_entry "$FILE" "$SERVICE"
done