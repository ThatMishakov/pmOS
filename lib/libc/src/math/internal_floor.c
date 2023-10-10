#include "internal_floor.h"

typedef unsigned DWORD;
typedef unsigned long QWORD;

// I don't understand any of this
// Taken from https://stackoverflow.com/questions/41856771/write-your-own-implementation-of-maths-floor-function-c

//---------------------------------------------------------------------------
// IEEE 754 double MSW masks
const DWORD _f64_sig    =0x80000000;    // sign
const DWORD _f64_exp    =0x7FF00000;    // exponent
const DWORD _f64_exp_sig=0x40000000;    // exponent sign
const DWORD _f64_exp_bia=0x3FF00000;    // exponent bias
const DWORD _f64_exp_lsb=0x00100000;    // exponent LSB
const DWORD _f64_exp_pos=        20;    // exponent LSB bit position
const DWORD _f64_man    =0x000FFFFF;    // mantisa
const DWORD _f64_man_msb=0x00080000;    // mantisa MSB
const DWORD _f64_man_bits=       52;    // mantisa bits
// IEEE 754 single masks
const DWORD _f32_sig    =0x80000000;    // sign
const DWORD _f32_exp    =0x7F800000;    // exponent
const DWORD _f32_exp_sig=0x40000000;    // exponent sign
const DWORD _f32_exp_bia=0x3F800000;    // exponent bias
const DWORD _f32_exp_lsb=0x00800000;    // exponent LSB
const DWORD _f32_exp_pos=        23;    // exponent LSB bit position
const DWORD _f32_man    =0x007FFFFF;    // mantisa
const DWORD _f32_man_msb=0x00400000;    // mantisa MSB
const DWORD _f32_man_bits=       23;    // mantisa bits
// IEEE 754 quadruple MSW masks
const QWORD _f128_sig    = 0x8000000000000000UL; // sign
const QWORD _f128_exp    = 0x7FFF000000000000UL; // exponent
const QWORD _f128_exp_sig= 0x4000000000000000UL; // exponent sign
const QWORD _f128_exp_bia= 0x3FFE000000000000UL; // exponent bias
const QWORD _f128_exp_lsb= 0x0001000000000000UL; // exponent LSB
const QWORD _f128_exp_pos= 49; // exponent LSB bit position
const QWORD _f128_man    = 0x0000FFFFFFFFFFFFUL; // mantissa
const QWORD _f128_man_msb= 0x0000800000000000UL; // mantissa MSB
const QWORD _f128_man_bits= 112; // mantissa bits
//---------------------------------------------------------------------------
float __internal_floorf(float x)
    {
    union               // semi result
        {
        float f;        // 32bit floating point
        DWORD u;        // 32 bit uint
        } y;
    DWORD m,a;
    int sig,exp,sh;
    y.f=x;
    // extract sign
    sig =y.u&_f32_sig;
    // extract exponent
    exp =((y.u&_f32_exp)>>_f32_exp_pos)-(_f32_exp_bia>>_f32_exp_pos);
    // floor bit shift
    sh=_f32_man_bits-exp; a=0;
    if (exp<0)
        {
        a=y.u&_f32_man;
        if (sig) return -1.0;
        return 0.0;
        }
    if (sh>0)
        {
        if (sh<_f32_exp_pos) m=(0xFFFFFFFF>>sh)<<sh; else m=_f32_sig|_f32_exp;
        a|=y.u&(m^0xFFFFFFFF); y.u&=m;
        }
    if ((sig)&&(a)) y.f--;
    return y.f;
    }
//---------------------------------------------------------------------------
double __internal_floor(double x)
    {
    const int h=1;      // may be platform dependent MSB/LSB order
    const int l=0;
    union _f64          // semi result
        {
        double f;       // 64bit floating point
        DWORD u[2];     // 2x32 bit uint
        } y;
    DWORD m,a;
    int sig,exp,sh;
    y.f=x;
    // extract sign
    sig =y.u[h]&_f64_sig;
    // extract exponent
    exp =((y.u[h]&_f64_exp)>>_f64_exp_pos)-(_f64_exp_bia>>_f64_exp_pos);
    // floor bit shift
    sh=_f64_man_bits-exp; a=0;
    if (exp<0)
        {
        a=y.u[l]|(y.u[h]&_f64_man);
        if (sig) return -1.0;
        return 0.0;
        }
    // LSW
    if (sh>0)
        {
        if (sh<32) m=(0xFFFFFFFF>>sh)<<sh; else m=0;
        a=y.u[l]&(m^0xFFFFFFFF); y.u[l]&=m;
        }
    // MSW
    sh-=32;
    if (sh>0)
        {
        if (sh<_f64_exp_pos) m=(0xFFFFFFFF>>sh)<<sh; else m=_f64_sig|_f64_exp;
        a|=y.u[h]&(m^0xFFFFFFFF); y.u[h]&=m;
        }
    if ((sig)&&(a)) y.f--;
    return y.f;
    }
//---------------------------------------------------------------------------
long double __internal_floorl(long double x)
    {
    const int h=1;      // may be platform dependent MSB/LSB order
    const int l=0;
    union {
        long double f;
        QWORD u[2];
    } y;
    QWORD m, a;
    int sig, exp, sh;    
    y.f = x;
    // extract sign
    sig = y.u[1] & _f128_sig;
    // extract exponent
    exp = ((y.u[1] & _f128_exp) >> _f128_exp_pos) - (_f128_exp_bia >> _f128_exp_pos);
    // floor bit shift
    sh=_f128_man_bits-exp; a=0;
    if (exp<0)
        {
        a=y.u[l]|(y.u[h]&_f128_man);
        if (sig) return -1.0;
        return 0.0;
        }
    // LSW
    if (sh>0)
        {
        if (sh<64) m=(0xFFFFFFFFFFFFFFFFUL>>sh)<<sh; else m=0;
        a=y.u[l]&(m^0xFFFFFFFFFFFFFFFFUL); y.u[l]&=m;
        }
    // MSW
    sh-=64;
    if (sh>0)
        {
        if (sh<_f128_exp_pos) m=(0xFFFFFFFFFFFFFFFFUL>>sh)<<sh; else m=_f128_sig|_f128_exp;
        a|=y.u[h]&(m^0xFFFFFFFFFFFFFFFFUL); y.u[h]&=m;
        }
    if ((sig)&&(a)) y.f--;
    return y.f;
    }