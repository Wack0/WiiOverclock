#pragma once
//#define VEGAS // to build for Vegas uncomment this
#ifdef VEGAS

// this is the default clock setting 243mhz
#define CLOCK_FEEDBACK 0x47
#define CLOCK_REFERENCE 0
#define CLOCK_OUTPUT 0x10

#define CLOCK_HZ ((u32)(27000000ull * (((CLOCK_FEEDBACK + 1) * 2) / (CLOCK_REFERENCE + 1))  / (CLOCK_OUTPUT)))

#else // BOLLYWOOD

//#define CLOCK_FEEDBACK 0x6c0000 // 243mhz default
#define CLOCK_FEEDBACK 0x8c0000 // 315mhz
#define CLOCK_REFERENCE 2
#define CLOCK_OUTPUT 8

_Static_assert(CLOCK_OUTPUT >= 2);
_Static_assert((CLOCK_OUTPUT & 1) == 0);

#define CLOCK_HZ ((u32)(27000000ull * (CLOCK_FEEDBACK) / (CLOCK_REFERENCE+1) / (CLOCK_OUTPUT/2) / 0x10000))

#define PLLEXT_VAL_CLKF ((CLOCK_FEEDBACK >> 1) & 0x3fff)

#endif

#define PLL_VAL_CLKO ((CLOCK_OUTPUT) & 0x1ff)
#define PLL_VAL_CLKF ((CLOCK_FEEDBACK >> 16) & 0xfff)
#define PLL_VAL_CLKR ((CLOCK_REFERENCE) & 0x3f)
#define PLL_REG_VAL ((PLL_VAL_CLKR << 0) | (PLL_VAL_CLKF << 6) | (PLL_VAL_CLKO << 18))

#define CLOCK_MHZ (CLOCK_HZ / 1000000)
#define CLOCK_MHZ_ROUND (CLOCK_MHZ + ((CLOCK_HZ % 1000000) >= 500000 ? 1 : 0))

#define CLOCK_SI_DIVISOR (((CLOCK_MHZ * 99) / 162) + ((CLOCK_MHZ % 162) != 0 ? 1 : 0))
