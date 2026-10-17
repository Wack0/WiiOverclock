/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.

Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>
Copyright (C) 2009		John Kelley <wiidev@kelley.ca>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "utils.h"
#include "start.h"
#include "hollywood.h"
#include "sdhc.h"
#include "string.h"
#include "memory.h"
#include "gecko.h"
#include "ff.h"
#include "panic.h"
#include "powerpc_elf.h"
#include "irq.h"
#include "ipc.h"
#include "exception.h"
#include "crypto.h"
#include "nand.h"
#include "boot2.h"
#include "dram.h"
#include "git_version.h"
#include "clock.h"

#define PPC_BOOT_FILE "/bootmii/ppcboot.elf"

FATFS fatfs;

u32 _main(void *base)
{
	FRESULT fres;
	int res;
	u32 vector;
	(void)base;

	gecko_init();
	gecko_printf("mini %s loading\n", git_version);
	
	// Do not switch clocks in gamecube mode.
	if ((read32(HW_SYSCTRL) & HW_SYSCTRL_GAMECUBE) == 0) {
		
		if ((u8)read32(HW_VERSION) < 0x20) {
#ifndef VEGAS
			gecko_printf("this is a vegas system, only bollywood is supported\n");
			while (1);
#endif
		}
#ifdef VEGAS
		else {
			gecko_printf("this is a bollywood system, only vegas is supported\n");
			while (1);
		}
#endif
		
		// We are running out of sram right now.
		// Put powerpc in reset.
		powerpc_hang();
		// Put old memory controller(???) and SI into reset.
		clear32(HW_RESETS, 0x48000);
		// Put some new blocks into reset
		clear32(HW_RESET_AHB, 0x438e);
		
		// Switch clocks, this will kill ddr+napa.
		// Change sysclk to other clock (this is configured for 162mhz default)
		set32(HW_SYSCTRL, 1);
		udelay(2);
		// put dskpll in reset
		clear32(HW_RESETS, (1<<3));
		udelay(10);
		// unlock syspll
		clear32(HW_RESETS, (1<<7));
		udelay(2);
		// Do the actual clock switch.
		u32 clk = read32(HW_PLLSYS);
		u32 oldclk = clk;
		clk = PLL_REG_VAL;
		write32(HW_PLLSYS, clk);
		udelay(20000);
#ifndef VEGAS // clkext part was added in bollywood
		u32 clkext = read32(HW_PLLSYSEXT);
		clkext &= ~0x007ffe00;
		clkext |= (PLLEXT_VAL_CLKF << 9);
		write32(HW_PLLSYSEXT, clkext);
		udelay(20000);
#endif
		// reset syspll to change the clocks
		clear32(HW_RESETS, (1<<6));
		udelay(2);
		set32(HW_RESETS, (1<<6));
		udelay(2);
		// relock syspll
		set32(HW_RESETS, (1<<7));
		udelay(2);
		// take dskpll out of reset
		set32(HW_RESETS, (1<<3));
		udelay(10);
		// switch clocks back to normal sysclk which we just reconfigured.
		clear32(HW_SYSCTRL, 1);
		udelay(2);
		
		// Change SI clock divider
		//write32(HW_SICLKDIV, 170); // would be 198 for 324mhz.
		write32(HW_SICLKDIV, CLOCK_SI_DIVISOR); 
		
		gecko_printf("upclocked,old=%08x,new=%08x\n", oldclk, read32(HW_PLLSYS));
		gecko_printf("%dhz, %dmhz\n", CLOCK_HZ, CLOCK_MHZ);
		
		init_mem();
		
		// Take old memory controller(???) and SI out of reset.
		set32(HW_RESETS, 0x48000);
		// Take some new blocks out of reset
		set32(HW_RESET_AHB, 0x438e);
		// Make sure AES/SHA/NAND can access SRAM
		write32(HW_MEMMIRR, 0x27);
		// Reinitialise the NAND controller properly now it's out of reset.
		set32(NAND_CONF, 0x8000000);
		write32(NAND_DATA, 0);
		write32(NAND_ECC, 0);
		write32(NAND_ADDR0, 0);
		write32(NAND_ADDR1, 0);
		write32(NAND_CMD, 0x80FF8000);
		gecko_printf("rest of ram up\n");
		
		// wipe bss2 again now ddr is up
		extern u32 __bss2_start[], __bss2_end[];
		for (u32* bss2 = __bss2_start; bss2 < __bss2_end; bss2++) *bss2 = 0;
	}
	

	gecko_printf("Initializing exceptions...\n");
	exception_initialize();
	gecko_printf("Configuring caches and MMU...\n");
	mem_initialize();

	gecko_printf("IOSflags: %08x %08x %08x\n",
		read32(0xffffff00), read32(0xffffff04), read32(0xffffff08));
	gecko_printf("          %08x %08x %08x\n",
		read32(0xffffff0c), read32(0xffffff10), read32(0xffffff14));

	irq_initialize();
//	irq_enable(IRQ_GPIO1B);
	irq_enable(IRQ_GPIO1);
	irq_enable(IRQ_RESET);
	irq_enable(IRQ_TIMER);
	irq_set_alarm(20, 1);
	gecko_printf("Interrupts initialized\n");
	
	// debug_read_otp();
	// while (1);

	crypto_initialize();
	gecko_printf("crypto support initialized\n");

	nand_initialize();
	gecko_printf("NAND initialized.\n");

	boot2_init();

	gecko_printf("Initializing IPC...\n");
	ipc_initialize();

	gecko_printf("Initializing SDHC...\n");
	sdhc_init();

	gecko_printf("Mounting SD...\n");
	fres = f_mount(0, &fatfs);

	if (read32(HW_SYSCTRL) & 2) {
		gecko_printf("GameCube compatibility mode detected...\n");
		vector = boot2_run(1, 0x101);
		goto shutdown;
	}

	if(fres != FR_OK) {
		gecko_printf("Error %d while trying to mount SD\n", fres);
		panic2(0, PANIC_MOUNT);
	}
	
	{
		res = babelfish_read();
		if (res != 0) gecko_printf("babelfish failed to read (%d), ios will have timing issues!\n", res);
	}

	gecko_printf("Trying to boot:" PPC_BOOT_FILE "\n");

	res = powerpc_boot_file(PPC_BOOT_FILE);
	if(res < 0) {
		gecko_printf("Failed to boot PPC: %d\n", res);
		gecko_printf("Booting System Menu\n");
		vector = boot2_run(1, 2);
		goto shutdown;
	}

	gecko_printf("Going into IPC mainloop...\n");
	vector = ipc_process_slow();
	gecko_printf("IPC mainloop done!\n");
	gecko_printf("Shutting down IPC...\n");
	ipc_shutdown();

shutdown:
	gecko_printf("Shutting down interrupts...\n");
	irq_shutdown();
	gecko_printf("Shutting down caches and MMU...\n");
	mem_shutdown();

	gecko_printf("Vectoring to 0x%08x...\n", vector);
	return vector;
}

