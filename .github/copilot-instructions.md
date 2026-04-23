# Copilot instructions for pmOS

## Build, run, and test commands

This repository is built with **Jinx** (not CMake/Ninja at the repo root).

```bash
# one-time bootstrap
git submodule update --init --recursive
make jinx
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
# direct jinx flow (when iterating on one architecture/component)
mkdir -p build-x86_64
cd build-x86_64
../jinx init .. ARCH=x86_64
../jinx build limine-disk-image
```

```bash
# build a single component/package (recipe name from recipes/<name>)
cd build-x86_64
../jinx build ahcid
../jinx build bootstrapd
../jinx build tests
```

```bash
# after code changes, rebuild affected recipe(s), then use make qemu-x86
# (qemu-x86 already builds the needed disk image target)
cd build-x86_64
../jinx build <changed-recipe> [<changed-recipe>...]
cd ..
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

- No dedicated repo-wide lint target was found in root build entry points (`Makefile`, `Jinxfile`, `recipes/*`).
- C/C++ formatting style is defined in `.clang-format`.

## High-level architecture (cross-file view)

- pmOS is a **microkernel + userspace services** OS. Kernel provides low-level primitives (memory management, scheduling, interrupts, IPC, capabilities/rights), while most drivers/services run in userspace and communicate over IPC.
- Build orchestration is split across:
  - `Jinxfile` (global build settings, architecture mapping, boot service list),
  - `recipes/*` (one package per component),
  - `scripts/make_*` (image/config generation).
- Boot assembly flow:
  1. `BOOT_SERVICES` in `Jinxfile` defines which userspace services are included at boot.
  2. `recipes/limine-config` or `recipes/hyper-config` turns that list into bootloader module entries (`scripts/make_limine_cnf.sh`, `scripts/make_hyper_config.sh`).
  3. `recipes/limine-disk-image` / `recipes/hyper-disk-image` copy `kernel`, `bootstrapd`, each `service.elf`, and `service.yaml` into boot media (`scripts/make_limine_disk_image.sh`, `scripts/make_hyper_disk_image.sh`).
  4. `bootstrapd` parses loaded module metadata and `*.yaml` service configs, then starts or hooks services by `run_type`.
- Service declaration model:
  - Executable + YAML are paired (`/boot/<service>` and `/boot/<service>.yaml`).
  - YAML drives runtime behavior (`ALWAYS_ONCE`, `FOR_EACH_MATCH`, property publication/matching via pmbus).
  - Recipes are responsible for installing both binary and matching YAML into `/boot`.

## Repository-specific conventions

- **Cross-target toolchain naming is fixed**: CMake projects set compiler targets to `${TARGET_ARCH}-pmos` and compile with clang/clang++/llvm-ar.
- **Architecture identifiers are canonical** across make/Jinx/CMake/scripts: `x86_64`, `i686`, `riscv64`, `loongarch64`.
- **Default to one architecture (`x86_64`) for routine development/testing** unless the task explicitly requires another architecture.
- **Jinx target behavior matters**: after modifying a component, rebuild its recipe target(s), then run `make qemu-x86` (which rebuilds the x86_64 disk image target).
- **When changing which boot recipes are included** (e.g. editing `BOOT_SERVICES`), explicitly regenerate boot config before rebuilding image:
  - `cd build-x86_64 && ../jinx build limine-config` (or `../jinx build hyper-config` for i686/Hyper flow).
- **Component packaging pattern** for userspace daemons/drivers is consistent:
  - recipe at `recipes/<name>`
  - source at `userspace/<name>`
  - YAML at `userspace/<name>/<name>.yaml`
  - package step copies YAML to `${dest_dir}/boot/<name>.yaml`
- **Boot-time service wiring lives in config**, not in ad-hoc code paths:
  - update `BOOT_SERVICES` (Jinxfile) and/or service YAML run/match metadata when changing startup behavior.
- **Subtrees under `limine/Hyper` and both `uACPI` paths are upstream projects/submodules** with their own tests/workflows; prefer minimal, intentional edits there.
