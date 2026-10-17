/*
 *  mini - a Free Software replacement for the Nintendo/BroadOn IOS.
 *  DRAM initialisation.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *                2026          Rairii
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "dram.h"
#include "hollywood.h"
#include "clock.h"
#include "utils.h"
#include "string.h"

// Originally based on minute's cafe dram init;
// changed to match Wii boot1/bc, removing gamecube-mode stuff.

#define HW_VERSION8() (u8)read32(HW_VERSION)

static void ddr_mem_write16_impl(u16 addr, u16 data) {
	write16(MEM_DDR_REG_ADDR, addr);
	read16(MEM_DDR_REG_ADDR);
	write16(MEM_DDR_REG_VAL, data);
}

static void ddr_mem_write16(u32 addr, u16 data) {
	//ddr_mem_write16_impl((addr - MEM_REG_BASE) / 2, data);
	write16(addr, data);
}

static u16 ddr_mem_read16_impl(u16 addr) {
	write16(MEM_DDR_REG_ADDR, addr);
	read16(MEM_DDR_REG_ADDR);
	return read16(MEM_DDR_REG_VAL);
}

static u16 ddr_mem_read16(u32 addr) {
	//return ddr_mem_read16_impl((addr - MEM_REG_BASE) / 2);
	return read16(addr);
}


static void ddr_seq_write16(u16 seqAddr, u16 seqVal)
{
	ddr_mem_write16(MEM_SEQ_REG_ADDR, seqAddr);
	ddr_mem_read16(MEM_SEQ_REG_ADDR);
	ddr_mem_write16(MEM_SEQ_REG_VAL, seqVal);
}

static u16 ddr_seq_read16(u16 seqAddr)
{
	ddr_mem_write16(MEM_SEQ_REG_ADDR, seqAddr);
	ddr_mem_read16(MEM_SEQ_REG_ADDR);
	return ddr_mem_read16(MEM_SEQ_REG_VAL);
}

static inline __attribute__((always_inline)) void ddr_arb_write(u16 addr, u16 cmd, u32 delay) {
	ddr_mem_write16(MEM_ARB_EXADDR, addr);
	ddr_mem_write16(MEM_ARB_EXCMD, cmd);
	ddr_mem_write16(MEM_ARB_EXCMD, cmd | 1);
	ddr_mem_write16(MEM_ARB_EXCMD, cmd);
	if (delay != 0) udelay(delay);
}

static u32 read_some_di_register(void) {
	u32 exictrl = read32(HW_EXICTRL);
	if ((exictrl & 1) == 0) set32(HW_EXICTRL, 1);
	u32 val = read32(0x0d806024); // some DI register
	
	if ((exictrl & 1) == 0) write32(HW_EXICTRL, exictrl);
	return val;
}

static u32 init_mem_impl() {
	u8 hw_ver = HW_VERSION8();
	
	u16 pad_bit1 = (hw_ver < 0x10 && (read_some_di_register() & 4) != 0)
		? 0 : 2;
	u16 ddr_clock = 999;
	u32 sys_clock = CLOCK_MHZ_ROUND;
	
	if ((read32(HW_RESETS) & 0x800) != 0) {
		// memory controller is out of reset
		// so put ddr into reset
		ddr_seq_write16(DDR_SEQ_CKEEN, 0);
		ddr_seq_write16(DDR_SEQ_CKEDYN, 0);
		ddr_seq_write16(DDR_SEQ_RSTB, 0);	
		udelay(10);
	}
	
	// put memory controller into reset
	clear32(HW_RESETS, 0x800);
	udelay(100);
	
	// ???
	clear32(HW_DIFLAGS, 0x20);
	
	// put MEM1 into reset
	clear32(HW_RESETS, 0x100);
	udelay(10);
	
	// take memory controller out of reset
	set32(HW_RESETS, 0x800);
	udelay(5);
	
	// take MEM1 out of reset
	set32(HW_RESETS, 0x100);
	udelay(10);
	
	// toggle MEM1's reset line
	clear32(HW_RESETS, 0x100);
	udelay(100);
	set32(HW_RESETS, 0x100);
	udelay(5);
	
	// init DDR clock
	{
		enum {
			REFERENCE_CLOCK_MHZ = 27
		};
		int divref = ddr_clock / REFERENCE_CLOCK_MHZ;
		int max_feedback = (hw_ver < 0x20) ? 2000 : 1600;
		int reference, feedback, output, bypass;
		int half_feedback = max_feedback / 2;
		
		if (ddr_clock > half_feedback) {
			bypass = 1;
			output = 0;
		} else {
			bypass = 0;
			output = 4;
		}
		
		if ((ddr_clock % REFERENCE_CLOCK_MHZ) == 0) {
			reference = 0;
			if (ddr_clock > half_feedback) {
				feedback = divref - 1;
			} else {
				feedback = (divref * 2) - 1;
			}
		} else {
			reference = 26;
			if (ddr_clock > half_feedback) {
				feedback = ddr_clock - 1;
			} else {
				feedback = (ddr_clock * 2) - 1;
			}
		}
		
		clear32(HW_PLLDDREXT, 0xC0000000);
		udelay(100);
		u32 pllddr_value = reference |
			(feedback << 6) |
			(output << 18) |
			(bypass << 28);
		//asm volatile("");
		mask32(HW_PLLDDR, 0x17FFFFFF, pllddr_value);
		udelay(100);
		mask32(HW_PLLDDREXT, 0x80000000, 0x58000000);
		udelay(1000);
		set32(HW_PLLDDREXT, 0x80000000);
		udelay(1000);
	}
	
	ddr_mem_write16(MEM_COMPAT, 0x24);
	udelay(5);
	ddr_mem_write16(MEM_COMPAT, 0x20);
	
	ddr_seq_write16(DDR_SEQ_SYNC, 0);
	
	ddr_seq_write16(DDR_SEQ_PAD1, 0x0d09 | pad_bit1);
	udelay(50);
	ddr_seq_write16(DDR_SEQ_PAD1, 0x0509 | pad_bit1);
	udelay(50);
	
	u16 madj, sadj;
	if (hw_ver < 0x20) {
		// vegas
		madj = 0xf0f0;
		sadj = 0x1616;
	} else {
		madj = 0x7676;
		sadj = 0;
	}
	
	ddr_seq_write16(DDR_SEQ_MADJL, madj);
	ddr_seq_write16(DDR_SEQ_MADJH, madj);
	ddr_seq_write16(DDR_SEQ_SADJ0L, sadj);
	ddr_seq_write16(DDR_SEQ_SADJ0H, sadj);
	ddr_seq_write16(DDR_SEQ_SADJ1L, sadj);
	ddr_seq_write16(DDR_SEQ_SADJ1H, sadj);
	udelay(50);
	
	ddr_seq_write16(DDR_SEQ_PAD1, 0x0109 | pad_bit1);
	udelay(10);
	
	ddr_seq_write16(DDR_SEQ_PAD0, 0x8000);
	
	ddr_seq_write16(DDR_SEQ_NPLDLY, 0x0);
	
	ddr_mem_write16(MEM_COLMSK, 0x1ff);
	ddr_mem_write16(MEM_ROWMSK, 0xfff);
	ddr_mem_write16(MEM_BANKMSK, 7);
	
	ddr_mem_write16(MEM_RANKSEL, 1);
	ddr_mem_write16(MEM_ROWSEL, 4);
	ddr_mem_write16(MEM_COLSEL, 6);
	ddr_mem_write16(MEM_BANKSEL, 2);
	
	ddr_mem_write16(MEM_ARB_MISC, 0xeff);
	
	ddr_mem_write16(MEM_SEQRD_HWM, 8);
	ddr_mem_write16(MEM_SEQWR_HWM, 12);
	ddr_mem_write16(MEM_SEQCMD_HWM, 24);
	
	ddr_mem_write16(MEM_ARB_MAXWR, 6);
	ddr_mem_write16(MEM_ARB_MAXRD, 5);
	
	ddr_mem_write16(MEM_CPUAHM_WR_T, 5);
	ddr_mem_write16(MEM_DMAAHM_WR_T, 5);
	ddr_mem_write16(MEM_DMAAHM0_WR_T, 5);
	ddr_mem_write16(MEM_DMAAHM1_WR_T, 5);
	ddr_mem_write16(MEM_PI_WR_T, 5);
	ddr_mem_write16(MEM_PE_WR_T, 5);
	ddr_mem_write16(MEM_IO_WR_T, 5);
	ddr_mem_write16(MEM_DSP_WR_T, 5);
	ddr_mem_write16(MEM_ACC_WR_T, 5);
	
	ddr_seq_write16(DDR_SEQ_ODTDYN, 0);
	ddr_seq_write16(DDR_SEQ_ODTON, 0);
	ddr_seq_write16(DDR_SEQ_BL4, 0);
	ddr_seq_write16(DDR_SEQ_RANK2, 1);
	ddr_seq_write16(DDR_SEQ_DDR2, 0);
	ddr_seq_write16(DDR_SEQ_QSDEF, 1);
	ddr_seq_write16(DDR_SEQ_BANK4, 0);
	ddr_seq_write16(DDR_SEQ_NPLCONF, 8);
	
	ddr_seq_write16(DDR_SEQ_TRCDR, 7);
	ddr_seq_write16(DDR_SEQ_TRCDW, 4);
	ddr_seq_write16(DDR_SEQ_TCL, 7);
	ddr_seq_write16(DDR_SEQ_TRRD, 4);
	ddr_seq_write16(DDR_SEQ_TFAW, 24);
	ddr_seq_write16(DDR_SEQ_TRFC, 27);
	ddr_seq_write16(DDR_SEQ_TRC, 23);
	
	ddr_seq_write16(DDR_SEQ_IDLEST, 11);

	ddr_seq_write16(DDR_SEQ_TRDWR, 9);
	ddr_seq_write16(DDR_SEQ_TWRRD, 11);
	ddr_seq_write16(DDR_SEQ_TR2R, 6);
	
	ddr_seq_write16(DDR_SEQ_RDPR, 12);
	ddr_seq_write16(DDR_SEQ_WRPR, 23);
	
	ddr_seq_write16(DDR_SEQ_QSOE0, 0xfc00);
	ddr_seq_write16(DDR_SEQ_QSOE1, 0x1f);
	ddr_seq_write16(DDR_SEQ_QSOE2, 0);
	ddr_seq_write16(DDR_SEQ_QSOE3, 0);
	
	ddr_seq_write16(DDR_SEQ_TWL, 2);
	ddr_seq_write16(DDR_SEQ_RRL, 10);
	
	ddr_seq_write16(DDR_SEQ_NPLRD, 8);
	
	ddr_seq_write16(DDR_SEQ_RECEN0, 0x1fe0);
	ddr_seq_write16(DDR_SEQ_RECEN1, 0);
	
	if (hw_ver < 0x20) {
		// vegas
		ddr_seq_write16(DDR_SEQ_STR0, 0x7252);
		ddr_seq_write16(DDR_SEQ_STR1, 0x4a5e);
		ddr_seq_write16(DDR_SEQ_STR2, 0x7bde);
		ddr_seq_write16(DDR_SEQ_STR3, 0xde);
	} else {
		// bollywood
		ddr_seq_write16(DDR_SEQ_STR0, 0x4210);
		ddr_seq_write16(DDR_SEQ_STR1, 0x4210);
		ddr_seq_write16(DDR_SEQ_STR2, 0x4210);
		ddr_seq_write16(DDR_SEQ_STR3, 0xd0);
	}
	
	ddr_seq_write16(DDR_SEQ_APAD0, 0xcc);
	ddr_seq_write16(DDR_SEQ_APAD1, 0);
	ddr_seq_write16(DDR_SEQ_CKPAD0, 0xcc);
	ddr_seq_write16(DDR_SEQ_CKPAD1, 0);
	ddr_seq_write16(DDR_SEQ_CMDPAD0, 0xcc);
	ddr_seq_write16(DDR_SEQ_CMDPAD1, 0);
	ddr_seq_write16(DDR_SEQ_DQPAD0, 0x8ec);
	ddr_seq_write16(DDR_SEQ_DQPAD1, 0);
	ddr_seq_write16(DDR_SEQ_QSPAD0, 0x476);
	ddr_seq_write16(DDR_SEQ_QSPAD1, 0);
	
	u16 wr0, wr1;
	if (pad_bit1 == 0) {
		wr0 = 0x800f;
		wr1 = 7;
	} else {
		wr0 = wr1 = 0;
	}
	
	ddr_seq_write16(DDR_SEQ_WRDQ0, wr0);
	ddr_seq_write16(DDR_SEQ_WRDQ1, wr1);
	ddr_seq_write16(DDR_SEQ_WRQS0, wr0);
	ddr_seq_write16(DDR_SEQ_WRQS1, wr1);
	
	// write the wrdqx/wrqsx values to end of sram for some reason?
	write32(0xffffff04, ddr_seq_read16(DDR_SEQ_WRDQ0));
	write32(0xffffff08, ddr_seq_read16(DDR_SEQ_WRDQ1));
	write32(0xffffff0c, ddr_seq_read16(DDR_SEQ_WRQS0));
	write32(0xffffff10, ddr_seq_read16(DDR_SEQ_WRQS1));
	write32(0xffffff14, 0xcafebabe);
	
	ddr_seq_write16(DDR_SEQ_WR, 0);
	
	ddr_mem_write16(MEM_COMPAT, 0);
	
	udelay(5);
	
	ddr_seq_write16(DDR_SEQ_CKEEN, 1);
	udelay(5);
	
	ddr_seq_write16(DDR_SEQ_RSTB, 1);
	udelay(200);
	
	ddr_seq_write16(DDR_SEQ_SYNC, 0);
	
	ddr_seq_write16(DDR_SEQ_RECVON, 1);
	
	ddr_arb_write(0xffff, 0x20, 1);
	
	ddr_arb_write(0x2882, 0x22, 5);
	
	ddr_arb_write(0x2882, 0x24, 5);
	
	ddr_arb_write(0x2c82, 0x22, 5);
	
	u8 vendorId0 = (u8) ddr_seq_read16(DDR_SEQ_VENDORID0);
	
	ddr_arb_write(0x2882, 0x22, 5);
	
	ddr_arb_write(0x2c82, 0x24, 5);
	
	u8 vendorId1 = (u8) (ddr_seq_read16(DDR_SEQ_VENDORID0) >> 8);
	
	ddr_arb_write(0x2882, 0x24, 5);
	
	ddr_arb_write(0x0903, 0x22, 5);
	
	ddr_arb_write(0x0903, 0x24, 1);
	
	ddr_seq_write16(DDR_SEQ_RECVON, 0);
	
	gecko_printf("ddr vendor: %02x,%02x\n", vendorId0, vendorId1);
	if (vendorId0 == 0xFF && vendorId1 == 0xFF) {
		gecko_printf("hang on, zero RAM???\n");
		gecko_printf("trying again\n");
		return 0;
	}
	
	// Original boot1/bc uses init tables here
	// We aren't going to support any more RAM types lol
	
	// Use two RAM chips if both are the same vendor id
	
	u32 vendor_type = vendorId0 & 0xF;
	if (vendor_type != 1 && vendor_type != 2) {
		if (vendor_type != 6) vendor_type = 0;
		else vendor_type = 3;
	}
	// 0=unk/other, 1=samsung, 2=infineon, 3=hynix
	
	ddr_seq_write16(DDR_SEQ_CKEEN, 0);
	ddr_seq_write16(DDR_SEQ_RSTB, 0);
	udelay(200);
	
	{
		u16 value = (vendorId0 == vendorId1 && vendor_type != 2)
			? 1 : 0;
		ddr_seq_write16(DDR_SEQ_CKEEN, value);
		udelay(5);
	}
	
	ddr_seq_write16(DDR_SEQ_RSTB, 1);
	udelay(200);
	
	ddr_seq_write16(DDR_SEQ_CKEEN, 1);
	
	ddr_seq_write16(DDR_SEQ_SYNC, 0);
	u16 twoRankBit = (vendorId0 == vendorId1) ? 4 : 0;
	if (twoRankBit == 0) {
		// using only single chip as vendor id does not match
		ddr_mem_write16(MEM_RANKSEL, 7);
		ddr_seq_write16(DDR_SEQ_RANK2, 0);
	}
	
	{
		u16 val22 = 0x208a | twoRankBit;
		if (vendor_type != 2) val22 |= 0x800;
		ddr_arb_write(0xFFFF, 0x20, 5);
		if (vendor_type == 2) {
			ddr_arb_write(0x4000, 0x22, 5);
			if (twoRankBit != 0) ddr_arb_write(0x4000, 0x24, 5);
		}
		ddr_arb_write(val22, 0x22, 5);
		if (twoRankBit != 0) ddr_arb_write(val22, 0x24, 5);
		ddr_arb_write(0x903, 0x22, 5);
		if (twoRankBit != 0) ddr_arb_write(0x903, 0x24, 5);
		udelay(100);
		ddr_arb_write(0xffff, 0x20, 5);
		ddr_arb_write(0xffff, 0x2, 5);
		ddr_arb_write(0xffff, 0x2, 5);
	}
	
	ddr_seq_write16(DDR_SEQ_RECVON, 1);
	ddr_seq_write16(DDR_SEQ_SYNC, 0);
	
	write16(MEM_MARR0_START, 0);
	write16(MEM_MARR0_END, 0);
	write16(MEM_MARR1_START, 0);
	write16(MEM_MARR1_END, 0);
	write16(MEM_MARR2_START, 0);
	write16(MEM_MARR2_END, 0);
	write16(MEM_MARR3_START, 0);
	write16(MEM_MARR3_END, 0);
	
	write16(MEM_REFRESH, ((4016 * sys_clock) / 10000) - 1);
	
	ddr_seq_write16(DDR_SEQ_CKEEN, 0);
	ddr_seq_write16(DDR_SEQ_CKEDYN, 1);
	ddr_mem_write16(MEM_REFRESH_FLAG, (sys_clock * 39) / 10);
	
	ddr_mem_write16(MEM_2CA, 0x29);
	ddr_mem_write16(MEM_2C8, vendorId0);
	ddr_mem_write16(MEM_2CA, 0x2b);
	ddr_mem_write16(MEM_2C8, vendorId1);
	
	write32(0xffffff00, (vendorId1 << 8) | vendorId0);
	
	return 1;
}

void init_mem(void) {
	while (!init_mem_impl()) {}
}