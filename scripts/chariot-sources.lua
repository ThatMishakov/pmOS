return {
    ["libyaml"] = function()
        return Source {
            name = "libyaml",
            Git("https://github.com/yaml/libyaml.git", "0.2.5"),
        }
    end,
    ["userspace/ahcid"] = function()
        return Source {
            name = "ahcid",
            Local("userspace/ahcid"),
        }
    end,
    ["bash"] = function()
        return Source {
            name = "bash",
            Archive("https://ftpmirror.gnu.org/gnu/bash/bash-5.3.tar.gz", "0d5cd86965f869a26cf64f4b71be7b96f90a3ba8b3d74e27e8e9d9d5550f31ba"),
            patches = { "recipes/bash/patches/0001-support-config.sub-add-pmos.patch" },
        }
    end,
    ["binutils"] = function()
        return Source {
            name = "binutils",
            Git("https://gitlab.com/mishakov/binutils-gdb-pmos.git", "f10f0b9d76933b662ec66ae33422160c649acc38"),
        }
    end,
    ["userspace/blockd"] = function()
        return Source {
            name = "blockd",
            Local("userspace/blockd"),
        }
    end,
    ["userspace/bootstrapd"] = function()
        return Source {
            name = "bootstrapd",
            Local("userspace/bootstrapd"),
        }
    end,
    ["clang"] = function()
        return Source {
            name = "clang",
            Git("https://gitlab.com/mishakov/llvm-pmos.git", "dbc6c8cbd5f0836361244c6b968d5c50f4a28c03"),
        }
    end,
    ["coreutils"] = function()
        return Source {
            name = "coreutils",
            dependencies = { "autoconf", "automake" },
            -- Keep the release's bundled gettext/gnulib macros; only regenerate
            -- the configure/Automake outputs affected by our m4 patch.
            prepare = [[AUTOPOINT=true autoreconf -f]],
            Archive("https://ftpmirror.gnu.org/coreutils/coreutils-9.12.tar.xz", "a480198559733e9b3da999e90543ac6f888a2caa544d8d664c5a1f17e528e210"),
            patches = { "recipes/coreutils/patches/0001-build-aux-config.sub-add-pmos.patch", "recipes/coreutils/patches/0002-m4-mountlist-m4-add-pmos-dummy.patch", "recipes/coreutils/patches/0003-src-stat.c-pmos-statvfs-no-type.patch", "recipes/coreutils/patches/0004-lib-getlocalename_l-unsafe-pmos.patch" },
        }
    end,
    ["userspace/devicesd"] = function()
        return Source {
            name = "devicesd",
            Local("userspace/devicesd"),
        }
    end,
    ["userspace/ext4"] = function()
        return Source {
            name = "ext4",
            Local("userspace/ext4"),
        }
    end,
    ["frigg"] = function()
        return Source {
            name = "frigg",
            Git("https://github.com/managarm/frigg.git", "b9ce1199489f25add06d66c5cc355e2c17dbe027"),
        }
    end,
    ["gcc"] = function()
        return Source {
            name = "gcc",
            Git("https://gitlab.com/mishakov/gcc-pmos.git", "122a0d03acf08d064b3e51bd60a76429edcdd2e5"),
            patches = { "recipes/gcc/patches/0002-libstdc++-v3-pmos-shared.patch", "recipes/gcc/patches/0003-loongarch-pmos-bootstrap-crt.patch", "recipes/gcc/patches/0004-loongarch-pmos-driver-options.patch" },
        }
    end,
    ["scripts"] = function()
        return Source {
            name = "build-support",
            Local("scripts"),
        }
    end,
    ["hyper"] = function()
        return Source {
            name = "hyper",
            Git("https://github.com/UltraOS/Hyper.git", "707cf65b7aa864f58232012e4c409aeb85d00584"),
            dependencies = { "git", "ca-certificates" },
            prepare = [[git submodule update --init --recursive --depth=1]],
        }
    end,
    ["userspace/i8042"] = function()
        return Source {
            name = "i8042",
            Local("userspace/i8042"),
        }
    end,
    ["kernel"] = function()
        return Source {
            name = "kernel",
            Local("kernel"),
        }
    end,
    ["libsmarter"] = function()
        return Source {
            name = "libsmarter",
            Git("https://github.com/managarm/libsmarter.git", "810746abbf03db3d95e49754a137d4417e216832"),
        }
    end,
    ["limine"] = function()
        return Source {
            name = "limine",
            Archive("https://codeberg.org/Limine/Limine/releases/download/v10.8.5/limine-10.8.5.tar.gz", "fb9eb2734258a6afd35f129dac29c04879b96d2930af940f4960be20b3033ece"),
        }
    end,
    ["lisp"] = function()
        return Source {
            name = "lisp",
            Git("https://github.com/DeanoBurrito/lisp.git", "8abecff982fdce7d451082a361d68db303d08d1c"),
            dependencies = { "build-essential", "clang" },
            prepare = [[make -C "${SOURCE_DIR}" bootstrap]],
        }
    end,
    ["userspace/logd"] = function()
        return Source {
            name = "logd",
            Local("userspace/logd"),
        }
    end,
    ["mlibc"] = function()
        return Source {
            name = "mlibc",
            Local("mlibc"),
            dependencies = { "meson", "git", "ca-certificates" },
            prepare = [[
meson subprojects download freestnd-c-hdrs freestnd-cxx-hdrs
            ]],
        }
    end,
    ["ncurses"] = function()
        return Source {
            name = "ncurses",
            Archive("https://invisible-island.net/archives/ncurses/ncurses-6.5.tar.gz", "136d91bc269a9a5785e5f9e980bc76ab57428f604ce3e5a5a90cebc767971cc6"),
            patches = { "recipes/ncurses/patches/0001-config.sub-add-pmos-support.patch", "recipes/ncurses/patches/0002-configure-pmos-shared-libs.patch" },
        }
    end,
    ["userspace/ns16550"] = function()
        return Source {
            name = "ns16550",
            Local("userspace/ns16550"),
        }
    end,
    ["userspace/pmbus"] = function()
        return Source {
            name = "pmbus",
            Local("userspace/pmbus"),
        }
    end,
    ["libs/pmos-containers/"] = function()
        return Source {
            name = "pmos-containers",
            Local("libs/pmos-containers"),
        }
    end,
    ["libs/pmos-helpers"] = function()
        return Source {
            name = "pmos-helpers",
            Local("libs/pmos-helpers"),
        }
    end,
    ["libs/rust"] = function()
        return Source {
            name = "pmos-rust-lib",
            Local("libs/rust"),
        }
    end,
    ["libs/pmoscxx"] = function()
        return Source {
            name = "pmoscxx",
            Local("libs/pmoscxx"),
        }
    end,
    ["userspace/processd"] = function()
        return Source {
            name = "processd",
            Local("userspace/processd"),
        }
    end,
    ["userspace/ps2d"] = function()
        return Source {
            name = "ps2d",
            Local("userspace/ps2d"),
        }
    end,
    ["readline"] = function()
        return Source {
            name = "readline",
            Archive("https://ftpmirror.gnu.org/gnu/readline/readline-8.3.tar.gz", "fe5383204467828cd495ee8d1d3c037a7eba1389c22bc6a041f627976f9061cc"),
            patches = { "recipes/readline/patches/0001-support-config.sub-add-pmos.patch", "recipes/readline/patches/0002-shobj-conf-pmos-shared.patch", "recipes/readline/patches/0003-shlib-install-pmos-symlinks.patch" },
        }
    end,
    ["rust"] = function()
        return Source {
            name = "rust",
            Git("https://gitlab.com/mishakov/rust-pmos.git", "47e3153daf7cb3947fb019ade3b3562acc6adce0"),
            dependencies = { "git", "ca-certificates" },
            prepare = [[git submodule update --init --recursive --depth=1]],
        }
    end,
    ["userspace/terminald"] = function()
        return Source {
            name = "terminald",
            Local("userspace/terminald"),
        }
    end,
    ["userspace/tests"] = function()
        return Source {
            name = "tests",
            Local("userspace/tests"),
        }
    end,
    ["zsh"] = function()
        return Source {
            name = "zsh",
            Archive("https://www.zsh.org/pub/zsh-5.9.2.tar.xz", "36fa734374b44783582cec09bcd67822e2f992c779ec1624ab5596df078d2f81"),
            patches = { "recipes/zsh/patches/0001-config.sub-add-pmos-support.patch", "recipes/zsh/patches/0002-configure-pmos-shared-libs.patch" },
        }
    end,
}
