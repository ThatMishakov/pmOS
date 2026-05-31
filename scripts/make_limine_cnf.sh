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

write_bootstrap_entry_multiboot() {
    FILE="$1"

    cat <<EOF >> "$FILE"
    module_path: boot():/bootstrapd
    module_string: bootstrapd;bootstrap

EOF
}

write_service_entry_multiboot() {
    FILE="$1"
    NAME="$2"
    cat <<EOF >> "$FILE"
    module_path: boot():/${NAME}.elf
    module_string: /${NAME}.elf;file

    module_path: boot():/${NAME}.yaml
    module_string: /${NAME}.yaml;init-config

EOF
}


if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <output_file> <services> <arch>"
    exit 1
fi

FILE="$1"
SERVICES="$2"
ARCH="$3"
cat <<EOF > "$FILE"
timeout: 5
default_entry: 1
EOF

if [ "$ARCH" = "i686" ]; then
    # Multiboot2
    cat <<EOF >> "$FILE"
/pmos (Multiboot2)
    protocol: multiboot2

EOF

    write_kernel_entry "$FILE"
    write_bootstrap_entry_multiboot "$FILE"
    for SERVICE in $SERVICES; do
        write_service_entry_multiboot "$FILE" "$SERVICE"
    done
elif [ "$ARCH" = "x86_64" ]; then
    # Non-kaslr
    cat <<EOF >> "$FILE"
/pmos (limine protocol, KASLR OFF)
    protocol: limine
    kaslr: off

EOF

    write_kernel_entry "$FILE"
    write_bootstrap_entry "$FILE"
    for SERVICE in $SERVICES; do
        write_service_entry "$FILE" "$SERVICE"
    done

    # Multiboot2
    cat <<EOF >> "$FILE"
/pmos (Multiboot2)
    protocol: multiboot2

EOF

    write_kernel_entry "$FILE"
    write_bootstrap_entry_multiboot "$FILE"
    for SERVICE in $SERVICES; do
        write_service_entry_multiboot "$FILE" "$SERVICE"
    done
else
    # Non-kaslr
    cat <<EOF >> "$FILE"
/pmos (limine protocol, KASLR OFF)
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


/pmos (limine protocol, KASLR ON)
    protocol: limine
    kaslr: on

EOF

    write_kernel_entry "$FILE"
    write_bootstrap_entry "$FILE"
    for SERVICE in $SERVICES; do
        write_service_entry "$FILE" "$SERVICE"
    done
fi