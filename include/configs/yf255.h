/*
 * (C) Copyright 2002
 * Kyle Harris, Nexus Technologies, Inc. kharris@nexus-tech.net
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * Configuation settings for the LUBBOCK board.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <asm/arch/pxa-regs.h>
//#define CONFIG_SKIP_LOWLEVEL_INIT //zkj. if boot from RAM
/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_ARCH_YF255
#define CONFIG_PXA250		1	/* This is an PXA250 CPU    */
//#define CONFIG_LUBBOCK		1	/* on an LUBBOCK Board	    */
//#define CONFIG_MMC		1
#define BOARD_LATE_INIT		1
//#define CONFIG_DOS_PARTITION

#undef CONFIG_USE_IRQ			/* we don't need IRQ/FIQ stuff */

/*
 * Size of malloc() pool
 */
#define CFG_MALLOC_LEN	    (CFG_ENV_SIZE + 128*1024)
#define CFG_GBL_DATA_SIZE	128	/* size in bytes reserved for initial data */

/*
 * Hardware drivers
 */
#define CONFIG_DRIVER_LAN91C113 //zkj
#define CONFIG_SMC91111_BASE (PXA_CS3_PHYS+0x300) //zkj
#define CONFIG_SMC_USE_32_BIT	1

/*
 * select serial console configuration
 */
#define CONFIG_FFUART	       1       /* we use FFUART on LUBBOCK */

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

#define CONFIG_BAUDRATE		115200


/*
 * BOOTP options
 */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME


/*
 * Command line configuration.
 */
#include <config_cmd_default.h>

//#define CONFIG_CMD_MMC
//#define CONFIG_CMD_FAT
#define CONFIG_CMD_NET
#define CONFIG_CMD_PING


#define CONFIG_BOOTDELAY	3
#define CONFIG_ETHADDR		00:80:0f:26:0a:5b
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		192.168.2.30
#define CONFIG_SERVERIP		192.168.2.10
#define CONFIG_BOOTCOMMAND	"cp.b 0x80000 0xa0200000 0x100000; go 0xa0200000"//zkj "bootm 80000"
#define CONFIG_BOOTARGS		"root=/dev/mtdblock2 rootfstype=cramfs console=ttyS0,115200"
#define CONFIG_CMDLINE_TAG
#define CONFIG_TIMESTAMP

#if defined(CONFIG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	230400		/* speed to run kgdb serial port */
#define CONFIG_KGDB_SER_INDEX	2		/* which serial port to use */
#endif

/*
 * Miscellaneous configurable options
 */
#define CFG_HUSH_PARSER		1
#define CFG_PROMPT_HUSH_PS2	"> "

#define CFG_LONGHELP				/* undef to save memory		*/
#ifdef CFG_HUSH_PARSER
#define CFG_PROMPT		"YF255# "		/* Monitor Command Prompt */
#else
#define CFG_PROMPT		"=> "		/* Monitor Command Prompt */
#endif
#define CFG_CBSIZE		256		/* Console I/O Buffer Size	*/
#define CFG_PBSIZE (CFG_CBSIZE+sizeof(CFG_PROMPT)+16) /* Print Buffer Size */
#define CFG_MAXARGS		16		/* max number of command args	*/
#define CFG_BARGSIZE		CFG_CBSIZE	/* Boot Argument Buffer Size	*/
#define CFG_DEVICE_NULLDEV	1

#define CFG_MEMTEST_START	0xa0400000	/* memtest works on	*/
#define CFG_MEMTEST_END		0xa0800000	/* 4 ... 8 MB in DRAM	*/

#undef	CFG_CLKS_IN_HZ		/* everything, incl board info, in Hz */

#define CFG_LOAD_ADDR	(CFG_DRAM_BASE + 0x8000) /* default load address */

#define CFG_HZ			3686400		/* incrementer freq: 3.6864 MHz */
#define CFG_CPUSPEED		0x141 //0x4130_0000.  zkj. set YF255 core clock to 200(Turbo)/200(Run)/100(Memory) MHz

						/* valid baudrates */
#define CFG_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

#define CFG_MMC_BASE		0xF0000000

/*
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128*1024)	/* regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)	/* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)	/* FIQ stack */
#endif

/*
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1	   /* we have 2 banks of DRAM */
#define PHYS_SDRAM_1		0xa0000000 /* SDRAM Bank #1 */
#define PHYS_SDRAM_1_SIZE	0x04000000 /* 64 MB */

#define PHYS_FLASH_1		0x00000000 /* Flash Bank #1 */
#define PHYS_FLASH_SIZE		0x02000000 /* 32 MB */
#define PHYS_FLASH_BANK_SIZE	0x02000000 /* 32 MB Banks */
#define PHYS_FLASH_SECT_SIZE	0x00040000 /* 256 KB sectors (x2) */

#define CFG_DRAM_BASE		0xa0000000
#define CFG_DRAM_SIZE		0x04000000

#define CFG_FLASH_BASE		PHYS_FLASH_1

#define FPGA_REGS_BASE_PHYSICAL 0x08000000

/*
 * GPIO settings
 */
#if 0 //zkj
#define CFG_GPSR0_VAL		0x00000000 //0x40E0_0018:
#define CFG_GPSR1_VAL		0x00000000 //0x40E0_001C:
#define CFG_GPSR2_VAL		0x00000000 //0x40E0_0020:
#else
#define CFG_GPSR0_VAL		0x00008000 //0x40E0_0018:
#define CFG_GPSR1_VAL		0x00FF0382 //0x40E0_001C:
#define CFG_GPSR2_VAL		0x0001C000 //0x40E0_0020:
#endif

#define CFG_GPCR0_VAL		0x00000000 //0x40E0_0024:
#define CFG_GPCR1_VAL		0x00000000 //0x40E0_0028:
#define CFG_GPCR2_VAL		0x00000000 //0x40E0_002C:

#define CFG_GPDR0_VAL		0x00008000 //0x40E0_000C:
#define CFG_GPDR1_VAL		0x00FF0380 //0x40E0_0010:
#define CFG_GPDR2_VAL		0x0001C000 //0x40E0_0014:

#define CFG_GAFR0_L_VAL		0x80000000 //0x40E0_0054:
#define CFG_GAFR0_U_VAL		0x00000000 //0x40E0_0058:

#define CFG_GAFR1_L_VAL		0x000A8010 //0x40E0_005C:
#define CFG_GAFR1_U_VAL		0x0005AAAA //0x40E0_0060:

#define CFG_GAFR2_L_VAL		0x80000000 //0x40E0_0064:
#define CFG_GAFR2_U_VAL		0x00000002 //0x40E0_0068:

#if 0 //zkj
#define CFG_PSSR_VAL		0x00 //0x40F0_0004
#else
#define CFG_PSSR_VAL		0x30 //0x40F0_0004
#endif

/*
 * Memory settings
 */
#define CFG_MSC0_VAL		0x24F224F0 //0x4800_0008
#define CFG_MSC1_VAL		0x3FF93FF9 //0x4800_000C
#define CFG_MSC2_VAL		0x3FF93FF9 //0x4800_0010

/* MDCNFG: SDRAM Configuration Register
 *
 * [31:29]   000 - reserved
 * [28]      0	 - no SA1111 compatiblity mode
 * [27]      0   - latch return data with return clock
 * [26]      0   - alternate addressing for pair 2/3
 * [25:24]   00  - timings
 * [23]      0   - internal banks in lower partition 2/3 (not used)
 * [22:21]   00  - row address bits for partition 2/3 (not used)
 * [20:19]   00  - column address bits for partition 2/3 (not used)
 * [18]      0   - SDRAM partition 2/3 width is 32 bit
 * [17]      0   - SDRAM partition 3 disabled
 * [16]      0   - SDRAM partition 2 disabled
 * [15:13]   000 - reserved
 * [12]      1	 - SA1111 compatiblity mode
 * [11]      1   - latch return data with return clock
 * [10]      0   - no alternate addressing for pair 0/1
 * [09:08]   10  - tRP=2*MemClk CL=2 tRCD=2*MemClk tRAS=5*MemClk tRC=8*MemClk
 * [7]       1   - 4 internal banks in lower partition pair
 * [06:05]   10  - 13 row address bits for partition 0/1
 * [04:03]   01  - 9 column address bits for partition 0/1
 * [02]      0   - SDRAM partition 0/1 width is 32 bit
 * [01]      0   - disable SDRAM partition 1
 * [00]      1   - enable  SDRAM partition 0
 */
/* use the configuration above but disable partition 0 */
#define CFG_MDCNFG_VAL		0x00001AC9 //0x4800_0000

/* MDREFR: SDRAM Refresh Control Register
 *
 * [32:26] 0     - reserved
 * [25]    0     - K2FREE: not free running
 * [24]    0     - K1FREE: not free running
 * [23]    0     - K0FREE: not free running
 * [22]    0     - SLFRSH: self refresh disabled
 * [21]    0     - reserved
 * [20]    0     - APD: auto power down
 * [19]    0     - K2DB2: SDCLK2 is MemClk
 * [18]    0     - K2RUN: disable SDCLK2
 * [17]    0     - K1DB2: SDCLK1 is MemClk
 * [16]    1     - K1RUN: enable SDCLK1
 * [15]    1     - E1PIN: SDRAM clock enable
 * [14]    0     - K0DB2: SDCLK0 is MemClk
 * [13]    0     - K0RUN: disable SDCLK0
 * [12]    0     - E0PIN: disable SDCKE0
 * [11:00] 000000011000 - (64ms/8192)*MemClkFreq/32 = 24
 * 64ms/2^13＝64ms/8192 = 7.8125μs, then 7.8125μs x 100MHz / 32 = 0x018
 */
#define CFG_MDREFR_VAL		0x00018018 //0x4800_0004

#define CFG_MDMRS_VAL		0x00000000

/*
 * PCMCIA and CF Interfaces
 */
#define CFG_MECR_VAL		0x00000000 //0x4800_0014

#define CFG_MCMEM0_VAL		0x00010504 //0x4800_0028
#define CFG_MCMEM1_VAL		0x00010504 //0x4800_002C

#define CFG_MCATT0_VAL		0x00010504 //0x4800_0030
#define CFG_MCATT1_VAL		0x00010504 //0x4800_0034

#define CFG_MCIO0_VAL		0x00004715 //0x4800_0038
#define CFG_MCIO1_VAL		0x00004715 //0x4800_003C

/*
 * FLASH and environment organization
 */
#define CFG_MAX_FLASH_BANKS	1	/* max number of memory banks		*/
#define CFG_MAX_FLASH_SECT	128  /* max number of sectors on one chip    */

/* timeout values are in ticks */
#define CFG_FLASH_ERASE_TOUT	(25*CFG_HZ) /* Timeout for Flash Erase */
#define CFG_FLASH_WRITE_TOUT	(25*CFG_HZ) /* Timeout for Flash Write */

/* NOTE: many default partitioning schemes assume the kernel starts at the
 * second sector, not an environment.  You have been warned!
 */
#define	CFG_MONITOR_LEN		PHYS_FLASH_SECT_SIZE
#define CFG_ENV_IS_IN_FLASH	1
#define CFG_ENV_ADDR		(PHYS_FLASH_1 + PHYS_FLASH_SECT_SIZE)
#define CFG_ENV_SECT_SIZE	PHYS_FLASH_SECT_SIZE
#define CFG_ENV_SIZE		(PHYS_FLASH_SECT_SIZE / 16)


/*
 * FPGA Offsets
 */
#define WHOAMI_OFFSET		0x00
#define HEXLED_OFFSET		0x10
#define BLANKLED_OFFSET		0x40
#define DISCRETELED_OFFSET	0x40
#define CNFG_SWITCHES_OFFSET	0x50
#define USER_SWITCHES_OFFSET	0x60
#define MISC_WR_OFFSET		0x80
#define MISC_RD_OFFSET		0x90
#define INT_MASK_OFFSET		0xC0
#define INT_CLEAR_OFFSET	0xD0
#define GP_OFFSET		0x100

#endif	/* __CONFIG_H */
