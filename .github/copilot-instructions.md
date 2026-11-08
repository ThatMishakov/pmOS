# Copilot instructions for pmOS

## Build, run, and test commands

This repository is built with **Chariot** (not CMake/Ninja at the repo root).

```bash
# one-time bootstrap
git submodule update --init --recursive
./scripts/install-chariot.sh
```

```bash
# preferred default build/test path (single architecture to limit disk/toolchain churn)
make qemu-x86        # x86_64 image + run
```

Other architecture runs are available when explicitly needed:
```bash
make qemu            # riscv64 image + run
make qemu-i686       # i686 (Hyper bootloader) image + run
make qemu-loongarch64
```

```bash
# direct Chariot flow (from the repository root)
.chariot-tools/bin/chariot build --arch x86_64 limine-disk-image
```

```bash
# build a single component/package (recipes/<name>.lua)
.chariot-tools/bin/chariot build --arch x86_64 ahcid bootstrapd tests

# rebuild affected packages, then build the image and boot it
.chariot-tools/bin/chariot build --arch x86_64 <changed-package>
make qemu-x86
```

### Test commands

- There is no top-level host `ctest`/`make test` target in this repo.
- The in-OS tests binary is packaged by recipe `tests` and launched from `/tests.elf` via `userspace/tests/tests.yaml` (run type `ALWAYS_ONCE`). Practical workflow: build image and boot in QEMU (`make qemu-*`), then inspect serial output.
- Submodule-scoped ACPI tests exist at:
  - `kernel/uACPI/tests/run_tests.py`
  - `userspace/devicesd/uACPI/tests/run_tests.py`

Example (full uACPI suite in that subtree):
```bash
cd kernel/uACPI/tests
python3 run_tests.py
```

Example single-case pattern (point `--test-dir` to a directory containing only one `.asl` file):
```bash
cd kernel/uACPI/tests
python3 run_tests.py --test-dir /path/to/one-case-dir
```

### Lint/format

- No dedicated repo-wide lint target was found in root build entry points (`Makefile`, `chariot.lua`, `recipes/*.lua`).
- C/C++ formatting style is defined in `.clang-format`.

## High-level architecture (cross-file view)

- pmOS is a **microkernel + userspace services** OS. Kernel provides low-level primitives (memory management, scheduling, interrupts, IPC, capabilities/rights), while most drivers/services run in userspace and communicate over IPC.
- Build orchestration is split across:
  - `chariot.lua` (build graph, architecture selection and boot service list),
  - `chariot_config.toml` (pinned build environment),
  - `scripts/chariot-common.sh` (cross-compilation settings),
  - `recipes/*.lua` and `host-recipes/*.lua` (target packages and host tools),
  - `scripts/make_*` (image/config generation).
- Boot assembly flow:
  1. The service list in `chariot.lua` defines which userspace services are included at boot.
  2. `recipes/limine-config.lua` or `recipes/hyper-config.lua` turns that list into bootloader module entries (`scripts/make_limine_cnf.sh`, `scripts/make_hyper_config.sh`).
  3. `recipes/limine-disk-image.lua` / `recipes/hyper-disk-image.lua` copy `kernel`, `bootstrapd`, each `service.elf`, and `service.yaml` into boot media (`scripts/make_limine_disk_image.sh`, `scripts/make_hyper_disk_image.sh`).
  4. `bootstrapd` parses loaded module metadata and `*.yaml` service configs, then starts or hooks services by `run_type`.
- Service declaration model:
  - Executable + YAML are paired (`/boot/<service>` and `/boot/<service>.yaml`).
  - YAML drives runtime behavior (`ALWAYS_ONCE`, `FOR_EACH_MATCH`, property publication/matching via pmbus).
  - Recipes are responsible for installing both binary and matching YAML into `/boot`.

## Repository-specific conventions

- **Cross-target toolchain naming is fixed**: CMake projects set compiler targets to `${TARGET_ARCH}-pmos` and compile with clang/clang++/llvm-ar.
- **Architecture identifiers are canonical** across Make/Chariot/CMake/scripts: `x86_64`, `i686`, `riscv64`, `loongarch64`, `m68k` (the unfinished port is selectable through Chariot).
- **Primary regression baseline**: Clang builds of x86_64, i686, riscv64 and loongarch64 reaching Bash. Keep M68k and GCC selectable; known upstream LLVM bugs, missing M68k kernel support and existing GCC/libgcc/linker issues are separate from migration regressions.
- **Shared host tools**: keep Clang/LLVM, Hyper and Limine independent of target architecture and options. Compiler-rt and target runtimes remain per target. After building LLVM, `scripts/check-llvm-cache.sh` checks reuse across all profiles.
- **Default to one architecture (`x86_64`) for routine development/testing** unless the task explicitly requires another architecture.
- **Chariot target behavior matters**: after modifying a component, rebuild its recipe target(s), then run `make qemu-x86` (which rebuilds the x86_64 disk image target).
- **When changing boot services**, update the list in `chariot.lua`; rebuilding an image automatically regenerates its configuration. To inspect a config on its own, build `limine-config` or `hyper-config` with the desired `--arch`.
- **Component packaging pattern** for userspace daemons/drivers is consistent:
  - recipe at `recipes/<name>.lua`
  - source at `userspace/<name>`
  - YAML at `userspace/<name>/<name>.yaml`
  - package step copies YAML to `${INSTALL_DIR}/boot/<name>.yaml`
- **Boot-time service wiring lives in config**, not in ad-hoc code paths:
  - update the service list in `chariot.lua` and/or service YAML run/match metadata when changing startup behavior.
- **Subtrees under `limine/Hyper` and both `uACPI` paths are upstream projects/submodules** with their own tests/workflows; prefer minimal, intentional edits there.
