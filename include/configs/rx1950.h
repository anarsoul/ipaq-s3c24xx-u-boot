/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 * Gary Jennejohn <gj@denx.de>
 * David Mueller <d.mueller@elsoft.ch>
 *
 * Configuation settings for the Armzone QT2410 board.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CFG_UBOOT_SIZE            0x40000

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_ARM920T		1	/* This is an ARM920T Core	*/
#define CONFIG_SMDK2440		1	/* on a SAMSUNG SMDK2410 Board  */
#define	CONFIG_S3C2442		1	/* SAMSUNG S3C2442 SoC		*/

/* input clock of PLL */
#define CONFIG_SYS_CLK_FREQ	16934400 /* the rx1950 has 16.9344MHz input clock */

#define USE_920T_MMU		1
#define CONFIG_USE_IRQ		1

/*
 * Size of malloc() pool
 */
#define CFG_MALLOC_LEN		(CFG_ENV_SIZE + 400*1024)
#define CFG_GBL_DATA_SIZE	128	/* size in bytes reserved for initial data */

/*
 * select serial console configuration
 */
#define CONFIG_SERIAL1          1	/* we use SERIAL 1 on SMDK2410 */
#define CONFIG_BAUDRATE		115200

/***********************************************************
 * Command definition
 ***********************************************************/
#define CONFIG_CMD_BDI
#define CONFIG_CMD_LOADS
#define CONFIG_CMD_LOADB
#define CONFIG_CMD_IMI
#define CONFIG_CMD_CACHE
#define CONFIG_CMD_ENV
#define CONFIG_CMD_BOOTD
#define CONFIG_CMD_CONSOLE
#define CONFIG_CMD_ASKENV
#define CONFIG_CMD_RUN
#define CONFIG_CMD_ECHO
#define CONFIG_CMD_REGINFO
#define CONFIG_CMD_IMMAP
#define CONFIG_CMD_AUTOSCRIPT
#define CONFIG_CMD_BSP
#define CONFIG_CMD_ELF
#define CONFIG_CMD_MISC
#define CONFIG_CMD_JFFS2
#define CONFIG_CMD_DIAG
#define CONFIG_CMD_SAVES
#define CONFIG_CMD_NAND
#define CONFIG_CMD_PORTIO
#define CONFIG_CMD_MMC
#define CONFIG_CMD_FAT
#define CONFIG_CMD_EXT2

#define CONFIG_BOOTDELAY	5
#define CONFIG_BOOTARGS    	"root=/dev/mmcblk0p2 rootdelay=5 psplash=false"
#define CONFIG_ETHADDR		01:ab:cd:ef:fe:dc
#define CONFIG_NETMASK          255.255.255.0
#define CONFIG_IPADDR		10.0.0.110
#define CONFIG_SERVERIP		10.0.0.1

#define CONFIG_BOOTCOMMAND	"run bootcmd${bootindex}"

#define CONFIG_DOS_PARTITION	1

#if defined(CONFIG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	115200		/* speed to run kgdb serial port */
/* what's this ? it's not used anywhere */
#define CONFIG_KGDB_SER_INDEX	1		/* which serial port to use */
#endif

/*
 * Miscellaneous configurable options
 */
#define	CFG_LONGHELP				/* undef to save memory		*/
#define	CFG_PROMPT		"RX1950 # "	/* Monitor Command Prompt	*/
#define	CFG_CBSIZE		1024		/* Console I/O Buffer Size	*/
#define	CFG_PBSIZE (CFG_CBSIZE+sizeof(CFG_PROMPT)+16) /* Print Buffer Size */
#define	CFG_MAXARGS		64		/* max number of command args	*/
#define CFG_BARGSIZE		CFG_CBSIZE	/* Boot Argument Buffer Size	*/

#define CFG_MEMTEST_START	0x30000000	/* memtest works on	*/
#define CFG_MEMTEST_END		0x31F00000	/* 31 MB in DRAM	*/

#undef  CFG_CLKS_IN_HZ		/* everything, incl board info, in Hz */

#define	CFG_LOAD_ADDR		0x30100000	/* default load address	*/

/* the PWM TImer 4 uses a counter of 15625 for 10 ms, so we need */
/* it to wrap 100 times (total 1562500) to get 1 sec. */
#define	CFG_HZ			1562500

/* valid baudrates */
#define CFG_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

#define CFG_BOOTMENU_SIMPLE	1

/*-----------------------------------------------------------------------
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128*1024)	/* regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)	/* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)	/* FIQ stack */
#endif

#define CONFIG_USB_DEVICE	1
#define CONFIG_USB_TTY		1
#define CFG_CONSOLE_IS_IN_ENV	1
#define CONFIG_USBD_VENDORID		0x1d50     /* Linux/NetChip */
#define CONFIG_USBD_PRODUCTID_GSERIAL	0x5120    /* gserial */
#define CONFIG_USBD_PRODUCTID_CDCACM	0x5119    /* CDC ACM */
#define CONFIG_USBD_MANUFACTURER	"anarsoul"
#define CONFIG_USBD_PRODUCT_NAME	"rx1950 Bootloader " U_BOOT_VERSION
#define CONFIG_USBD_DFU			1
#define CONFIG_USBD_DFU_XFER_SIZE	4096
#define CONFIG_USBD_DFU_INTERFACE	2

#define CONFIG_EXTRA_ENV_SETTINGS \
	"usbtty=cdc_acm\0"\
	"stderr=usbtty\0stdout=usbtty\0stdin=usbtty\0"\
	"mtdparts=Internal:16k(Boot0),256k(Boot1),3M(Kernel),-(Filesystem)\0"\
	"bootcmd0=mmcinit; fatload mmc 1:1 0x30080000 rx1950.bl; bootstrap 0x30080000\0"\
	"bootcmd1=mmcinit; fatload mmc 1:1 0x31000000 uImage; bootm 0x31000000\0"\
	"bootindex=0\0"\
	""

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1	   /* we have 1 bank of DRAM */
#define PHYS_SDRAM_1		0x30000000 /* SDRAM Bank #1 */
#define PHYS_SDRAM_1_SIZE	0x02000000 /* 32 MB */

#define CFG_NO_FLASH		1
#define	CFG_ENV_IS_IN_NAND	1
#define CFG_ENV_OFFSET		0x40000
#define CFG_ENV_SIZE		0x4000		/* 16k Total Size of Environment Sector */

#define NAND_MAX_CHIPS		1
#define CFG_NAND_BASE		0x4e000000
#define CFG_MAX_NAND_DEVICE	1

#define CONFIG_MMC		1
#define CONFIG_MMC_S3C	1	/* Enabling the MMC driver */
#define CFG_MMC_BASE		0xff000000

#define CONFIG_EXT2		1
#define CONFIG_FAT		1
#define CONFIG_SUPPORT_VFAT

#if 1
/* JFFS2 driver */
#define CONFIG_JFFS2_CMDLINE	1
#define CONFIG_JFFS2_NAND	1
#define CONFIG_JFFS2_NAND_DEV	0
//#define CONFIG_JFFS2_NAND_OFF		0x634000
//#define CONFIG_JFFS2_NAND_SIZE	0x39cc000
#endif

/* ATAG configuration */
#define CONFIG_INITRD_TAG		1
#define CONFIG_SETUP_MEMORY_TAGS	1
#define CONFIG_CMDLINE_TAG		1

//#define CONFIG_SKIP_LOWLEVEL_INIT 1
#define CONFIG_S3C24X0_SKIP_LOWLEVEL_INIT 1

#define CONFIG_CMD_UNZIP

#define CONFIG_S3C2410_NAND_BBT	1
#define CONFIG_S3C2410_NAND_HWECC 1

#define MTDIDS_DEFAULT		"nand0=Internal"
#define MTPARTS_DEFAULT		"Internal:16k(Boot0),240k(Boot1),16k(Env),3M(Kernel),-(Filesystem)"
#define CFG_NAND_DYNPART_MTD_KERNEL_NAME "Internal"
#define CONFIG_NAND_DYNPART

#define BOARD_LATE_INIT			1

#if 0
#define CONFIG_VIDEO
#define CONFIG_VIDEO_S3C2410
#define CONFIG_CFB_CONSOLE
#define CONFIG_VGA_AS_SINGLE_DEVICE

#define VIDEO_FB_16BPP_PIXEL_SWAP

#define VIDEO_KBD_INIT_FCT	0
#define VIDEO_TSTC_FCT		serial_tstc
#define VIDEO_GETC_FCT		serial_getc

#define LCD_VIDEO_ADDR		0x31d00000
#endif

#endif	/* __CONFIG_H */
