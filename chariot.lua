-- Keep target names aligned with the kernel and the existing Make/QEMU interface.
local arch = chariot.target_arch
assert(({x86_64=true, i686=true, riscv64=true, loongarch64=true, m68k=true, aarch64=true})[arch],
    "unsupported architecture: " .. arch)
for key in pairs(chariot.options) do
    assert(key == "toolchain" or key == "kernel-arch", "unknown option: " .. key)
end
local toolchain = chariot.options.toolchain or "clang"
assert(toolchain == "clang" or toolchain == "gcc", "unsupported toolchain: " .. toolchain)

local kernel_arch = chariot.options["kernel-arch"] or arch
assert(kernel_arch == arch or (arch == "i686" and kernel_arch == "x86_64"),
    "unsupported kernel/userspace combination: " .. kernel_arch .. "/" .. arch)

local c = {
    kernel_arch = kernel_arch,
    kernel_binary_dir = "build-" .. kernel_arch .. "/kernel-" .. toolchain .. "/boot",
}
local packages, tools, sources, loading = {}, {}, {}, {}
local source_defs = require("scripts.chariot-sources")
-- Embed cross-file templates so their contents are hashed with each script.
local common = chariot.read_file("scripts/chariot-common.sh")
    :gsub("@MESON_CROSS_FILE@", function() return chariot.read_file("scripts/meson.cross-file") end)
    :gsub("@CMAKE_TOOLCHAIN@", function() return chariot.read_file("scripts/CMakeToolchain.txt") end)
local services = {"ahcid", "devicesd", "logd", "processd", "terminald", "blockd",
                  "ps2d", "tests", "ns16550", "pmbus", "ext4"}
if arch == "x86_64" or arch == "i686" then table.insert(services, "i8042") end

-- Helpers only assemble native references and script text. Chariot owns the graph,
-- source preparation, isolated builds, installation and cache invalidation.
function c.deps(entries)
    local result = {}
    local function append(entry)
        if type(entry) == "table" then
            for _, value in ipairs(entry) do append(value) end
        else
            table.insert(result, entry)
        end
    end
    append(entries)
    return result
end
function c.source(name)
    if not sources[name] then
        assert(source_defs[name], "missing source: " .. name)
        sources[name] = source_defs[name]()
    end
    return sources[name]
end
function c.script(body)
    return 'export PMOS_ARCH="' .. arch .. '" PMOS_TOOLCHAIN="' .. toolchain .. '"\n'
        .. 'export BOOT_SERVICES="' .. table.concat(services, ' ') .. '"\n'
        .. common .. '\n' .. body
end
local function load(kind, name, registry)
    if registry[name] then return registry[name] end
    local key = kind .. "/" .. name
    assert(not loading[key], "dependency cycle at " .. key)
    loading[key] = true
    registry[name] = require(kind .. "." .. name)(c)
    loading[key] = nil
    return registry[name]
end
function c.pkg(name) return load("recipes", name, packages) end
function c.tool(name) return load("host-recipes", name, tools) end
function c.compiler() return {c.tool(toolchain)} end
function c.toolchain()
    if toolchain == "gcc" then return {c.tool("binutils"), c.tool("gcc")} end
    return {c.tool("clang"), c.tool("compiler-rt")}
end
function c.cxx()
    return {c.pkg(toolchain == "gcc" and "libstdc++" or "libcxx")}
end
function c.services()
    local result = {}
    for _, name in ipairs(services) do table.insert(result, c.pkg(name)) end
    return result
end

-- Declare the same image families in their existing order.
c.pkg("limine-disk-image")
c.pkg("hyper-disk-image")
c.pkg("grub-iso")
c.pkg("hyper-iso")
c.pkg("limine-iso")
c.pkg("zsh")

-- The shared LLVM tool is also available when selecting the GCC profile.
c.tool("clang")

-- Optional development tools remain directly addressable from the CLI.
c.tool("xgcc")
c.tool("libstdc++")
c.tool("lisp")
