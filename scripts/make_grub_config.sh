#!/bin/sh

write_service_entry() {
    FILE="$1"
    NAME="$2"
    cat <<EOF >> "$FILE"
    module2 --nounzip /${NAME}.elf "/${NAME}.elf;file"
    module2 --nounzip /${NAME}.yaml "/${NAME}.yaml;init-config"

EOF
}

write_kernel_entry() {
    FILE="$1"
    cat <<EOF >> "$FILE"
    multiboot2 /kernel

EOF
}

write_bootstrap_entry() {
    FILE="$1"

    cat <<EOF >> "$FILE"
    module2 --nounzip /bootstrapd "/bootstrapd;bootstrap"

EOF
}

write_library_entry() {
    FILE="$1"
    NAME="$2"
    NAME=${NAME##*/}
    LIBPATH="$3"
    cat <<EOF >> "$FILE"
    module2 --nounzip /${NAME} "/${LIBPATH}/${NAME};file"

EOF
}

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <output_file> <services>"
    exit 1
fi

LIBPATH="usr/lib"

FILE="$1"
SERVICES="$2"
cat <<EOF > "$FILE"
set default="0"
set timeout=3

menuentry "pmOS" {
EOF

write_kernel_entry "$FILE"
write_bootstrap_entry "$FILE"
for SERVICE in $SERVICES; do
    write_service_entry "$FILE" "$SERVICE"
done
for LIBRARY in $LIBRARIES; do
    write_library_entry "$FILE" "$LIBRARY" $LIBPATH
done

cat <<EOF >> "$FILE"
}
EOF