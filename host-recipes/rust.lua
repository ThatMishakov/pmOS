return function(c)
    -- Target-independent: pmOS standard libraries are supplied by rust-std.
    return Tool {
        name = "rust",
        version = "1.96.0",
        revision = 1,
        source = c.source("rust"),
        dependencies = {
            "llvm-dev", "python3", "curl", "git", "cmake",
            "ninja-build", "pkg-config", "libssl-dev", "zlib1g-dev",
        },
        configure = [[
host="$(uname -m)-unknown-linux-gnu"
case "$(uname -m)" in
    i?86) host=i686-unknown-linux-gnu ;;
    arm*) host=armv7-unknown-linux-gnueabihf ;;
    riscv64) host=riscv64gc-unknown-linux-gnu ;;
    ppc64le) host=powerpc64le-unknown-linux-gnu ;;
esac
cat <<EOF > bootstrap.toml
change-id = 148671

[llvm]
download-ci-llvm = false
link-shared = true

[build]
submodules = false
host = ["${host}"]
target = ["${host}"]
build-dir = "${BUILD_DIR}"
docs = false

[target.${host}]
llvm-config = "/usr/bin/llvm-config"

[install]
prefix = "${PREFIX}"
sysconfdir = "${PREFIX}/etc"
EOF
        ]],
        build = [[
cd "${SOURCE_DIR}"
CARGO_HOME="${BUILD_DIR}/.cargo" ./x.py build --config "${BUILD_DIR}/bootstrap.toml" --stage 1 compiler/rustc library/std -j${PARALLELISM}
        ]],
        install = [[
cd "${SOURCE_DIR}"
CARGO_HOME="${BUILD_DIR}/.cargo" DESTDIR="${INSTALL_DIR}" ./x.py install --config "${BUILD_DIR}/bootstrap.toml" --stage 1 compiler/rustc library/std -j${PARALLELISM}
find "${INSTALL_DIR}" -name "*.old" -delete
        ]],
    }
end
