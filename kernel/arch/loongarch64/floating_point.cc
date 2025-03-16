#include <csr.hh>
#include <loongarch_asm.hh>
#include <processes/tasks.hh>

using namespace kernel;

enum class VectorInstructionsLevel {
    None,
    FP,
    LSX,
    LASX,
};

VectorInstructionsLevel fp = VectorInstructionsLevel::None;

constexpr u32 CPUCFG2_FP   = 1 << 0;
constexpr u32 CPUCFG2_LSX  = 1 << 6;
constexpr u32 CPUCFG2_LASX = 1 << 7;

void detect_supported_extensions()
{
    auto cpucfg2 = cpucfg(2);
    if (cpucfg2 & CPUCFG2_LASX)
        fp = VectorInstructionsLevel::LASX;
    else if (cpucfg2 & CPUCFG2_LSX)
        fp = VectorInstructionsLevel::LSX;
    else if (cpucfg2 & CPUCFG2_FP)
        fp = VectorInstructionsLevel::FP;
}

kresult_t TaskDescriptor::init_fp_state()
{
    if (is_system)
        return 0;

    size_t size = 0;
    switch (fp) {
    case VectorInstructionsLevel::FP:
        size = 2 + 32; // 32 64-bit registers
        break;
    case VectorInstructionsLevel::LSX:
        size = 2 + 64; // 32 128-bit registers
        break;
    case VectorInstructionsLevel::LASX:
        size = 2 + 128; // 32 256-bit registers
        break;
    default:
        return 0;
    }

    fp_registers = klib::unique_ptr<u64[]>(new u64[size](0));
    return fp_registers ? 0 : -ENOMEM;
}

extern "C" void loongarch_save_fp(u64 *ptr);
extern "C" void loongarch_save_lsx(u64 *ptr);
extern "C" void loongarch_save_lasx(u64 *ptr);
extern "C" void loongarch_restore_fp(u64 *ptr);
extern "C" void loongarch_restore_lsx(u64 *ptr);
extern "C" void loongarch_restore_lasx(u64 *ptr);

constexpr u32 EUEN_FPE  = 1 << 0;
constexpr u32 EUEN_SXE  = 1 << 1;
constexpr u32 EUEN_ASXE = 1 << 2;

constexpr u32 EXTENSIONS_MASK = EUEN_FPE | EUEN_SXE | EUEN_ASXE;

bool fp_is_enabled() { return csrrd32<loongarch::csr::EUEN>() & EXTENSIONS_MASK; }
void fp_disable() { csrxchg32<loongarch::csr::EUEN>(0, EXTENSIONS_MASK); }
void fp_enable()
{
    u32 mask = 0;
    switch (fp) {
    case VectorInstructionsLevel::FP:
        mask = EUEN_FPE;
        break;
    case VectorInstructionsLevel::LSX:
        mask = EUEN_FPE | EUEN_SXE;
        break;
    case VectorInstructionsLevel::LASX:
        mask = EUEN_FPE | EUEN_SXE | EUEN_ASXE;
        break;
    default:
        mask = 0;
        break;
    }

    csrxchg32<loongarch::csr::EUEN>(mask, EXTENSIONS_MASK);
}

void fp_save(u64 *data)
{
    assert(data);
    switch (fp) {
    case VectorInstructionsLevel::FP:
        loongarch_save_fp(data);
        break;
    case VectorInstructionsLevel::LSX:
        loongarch_save_lsx(data);
        break;
    case VectorInstructionsLevel::LASX:
        loongarch_save_lasx(data);
        break;
    default:
        assert(!"fp_save() on CPU without extensions");
    }
}

void fp_restore(u64 *data)
{
    assert(data);
    switch (fp) {
    case VectorInstructionsLevel::FP:
        loongarch_restore_fp(data);
        break;
    case VectorInstructionsLevel::LSX:
        loongarch_restore_lsx(data);
        break;
    case VectorInstructionsLevel::LASX:
        loongarch_restore_lasx(data);
        break;
    default:
        assert(!"fp_restore() on CPU without extensions");
    }
}

void TaskDescriptor::before_task_switch()
{
    if (fp_is_enabled()) {
        assert(using_fp);
        fp_save(fp_registers.get());
        fp_disable();
    }
}

void TaskDescriptor::after_task_switch()
{
    if (using_fp) {
        fp_enable();
        fp_restore(fp_registers.get());
    }
}

constexpr unsigned EXCEPTION_FPE  = 0x0f;
constexpr unsigned EXCEPTION_SXD  = 0x10;
constexpr unsigned EXCEPTION_ASXD = 0x11;

kresult_t handle_fp_disabled_exception(unsigned code)
{
    switch (code) {
    case EXCEPTION_FPE:
        if (fp < VectorInstructionsLevel::FP)
            return -ENOTSUP;
        break;
    case EXCEPTION_SXD:
        if (fp < VectorInstructionsLevel::LSX)
            return -ENOTSUP;
        break;
    case EXCEPTION_ASXD:
        if (fp < VectorInstructionsLevel::LASX)
            return -ENOTSUP;
        break;
    default:
        assert(false);
    }

    auto task      = get_current_task();
    task->using_fp = true;
    fp_enable();
    fp_restore(task->fp_registers.get());

    return 0;
}