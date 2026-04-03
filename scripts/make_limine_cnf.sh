#!/bin/sh

write_service_entry() {
    FILE="$1"
    NAME="$2"
    cat <<EOF >> "$FILE"
    module_path: boot():/${NAME}.elf
    module_cmdline: file

    module_path: boot():/${NAME}.yaml
    module_cmdline: init-config

EOF
}

write_kernel_entry() {
    FILE="$1"
    cat <<EOF >> "$FILE"
    kernel_path: boot():/kernel

EOF
}

write_bootstrap_entry() {
    FILE="$1"

    cat <<EOF >> "$FILE"
    module_path: boot():/bootstrapd
    module_cmdline: bootstrap

EOF
}

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <output_file> <services>"
    exit 1
fi

FILE="$1"
SERVICES="$2"
cat <<EOF > "$FILE"
timeout: 5
EOF

# Non-kaslr
cat <<EOF >> "$FILE"
/pmos (KASLR OFF)
    protocol: limine
    kaslr: off

EOF

write_kernel_entry "$FILE"
write_bootstrap_entry "$FILE"
for SERVICE in $SERVICES; do
    write_service_entry "$FILE" "$SERVICE"
done

# Kaslr
cat <<EOF >> "$FILE"


/pmos (KASLR ON)
    protocol: limine
    kaslr: on

EOF

write_kernel_entry "$FILE"
write_bootstrap_entry "$FILE"
for SERVICE in $SERVICES; do
    write_service_entry "$FILE" "$SERVICE"
done