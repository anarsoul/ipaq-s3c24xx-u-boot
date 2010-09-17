/*
 * (C) Copyright 2010
 * Vasily Khoruzhick <anarsoul@gmail.com>
 *
 * based on qt2410 code
 *
 * (C) 2006 by OpenMoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 *
 * based on existing S3C2410 startup code in u-boot:
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
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

#include <bootmenu_simple.h>
#include <common.h>
#include <video_fb.h>
#include <usbdcore.h>
#include <s3c2410.h>

DECLARE_GLOBAL_DATA_PTR;

#define H1940_LATCH 0x10000000

/* SD layer latch */

#define H1940_LATCH_SDQ1		(1<<16)
#define H1940_LATCH_LCD_P1		(1<<17)
#define H1940_LATCH_LCD_P2		(1<<18)
#define H1940_LATCH_LCD_P3		(1<<19)
#define H1940_LATCH_MAX1698_nSHUTDOWN	(1<<20)		/* LCD backlight */
#define H1940_LATCH_LED_RED		(1<<21)
#define H1940_LATCH_SDQ7		(1<<22)
#define H1940_LATCH_USB_DP		(1<<23)

/* CPU layer latch */

#define H1940_LATCH_UDA_POWER		(1<<24)
#define H1940_LATCH_AUDIO_POWER		(1<<25)
#define H1940_LATCH_SM803_ENABLE	(1<<26)
#define H1940_LATCH_LCD_P4			(1<<27)
#define H1940_LATCH_SD_POWER		(1<<28)
#define H1940_LATCH_BLUETOOTH_POWER	(1<<29)		/* active high */
#define H1940_LATCH_LED_GREEN		(1<<30)
#define H1940_LATCH_LED_FLASH		(1<<31)

/* default settings */

#define H1940_LATCH_DEFAULT		\
	H1940_LATCH_LCD_P4		| \
	H1940_LATCH_SM803_ENABLE	| \
	H1940_LATCH_SDQ1		| \
	H1940_LATCH_LCD_P1		| \
	H1940_LATCH_LCD_P2		| \
	H1940_LATCH_LCD_P3		| \
	H1940_LATCH_MAX1698_nSHUTDOWN   | \
	H1940_LATCH_SD_POWER

static u_int32_t h1940_latch = H1940_LATCH_DEFAULT;

static inline void delay (unsigned long loops)
{
	__asm__ volatile ("1:\n"
	  "subs %0, %1, #1\n"
	  "bne 1b":"=r" (loops):"0" (loops));
}

/*
 * 1 for green
 * 2 for red
 * 4 for blue
 */
void h1940_led_set(int value)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	if (value & 1) {
		h1940_latch |= H1940_LATCH_LED_GREEN;
		outl(h1940_latch, H1940_LATCH);
		gpio->GPADAT |= (1 << 7);
	} else {
		h1940_latch &= ~H1940_LATCH_LED_GREEN;
		outl(h1940_latch, H1940_LATCH);
		gpio->GPADAT &= ~(1 << 7);
	}

	if (value & 2) {
		h1940_latch |= H1940_LATCH_LED_RED;
		outl(h1940_latch, H1940_LATCH);
		gpio->GPADAT |= (1 << 1);
	} else {
		h1940_latch &= ~H1940_LATCH_LED_RED;
		outl(h1940_latch, H1940_LATCH);
		gpio->GPADAT &= ~(1 << 1);
	}

	if (value & 4) {
		h1940_latch |= H1940_LATCH_LED_FLASH;
		gpio->GPADAT |= (1 << 3);
	} else {
		h1940_latch &= ~H1940_LATCH_LED_FLASH;
		gpio->GPADAT &= ~(1 << 3);
	}
}

int h1940_up_pressed(void)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	if (gpio->GPGDAT & (1 << 9))
		return 0;
	return 1;
}

int h1940_down_pressed(void)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	if (gpio->GPGDAT & (1 << 10))
		return 0;
	return 1;
}

int h1940_enter_pressed(void)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	if (gpio->GPFDAT & (1 << 7))
		return 0;
	return 1;
}

static struct bootmenu_simple_setup h1940_bm_setup = {
	.next_key = h1940_down_pressed,
	.prev_key = h1940_up_pressed,
	.select_key = h1940_enter_pressed,
	.print_index = h1940_led_set,
};

/*
 * Miscellaneous platform dependent initialisations
 */

int board_init (void)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	/* Enable latch chip select */
	gpio->GPACON |= (1 << 13);

	/* Configure mmc pins to function mode */
	gpio->GPECON = 0xa52aa955;

	/* Disable write protect */
	gpio->GPADAT |= (1 << 0);

	/* arch number of SMDK2410-Board */
	gd->bd->bi_arch_number = MACH_TYPE_H1940;

	/* adress of boot parameters */
	gd->bd->bi_boot_params = 0x30100100;

	/* icache is already enabled, don't reenable it */
	dcache_enable();
	icache_enable();

	return 0;
}

int board_late_init(void)
{
	bootmenu_simple_init(&h1940_bm_setup);

	return 0;
}

#if defined(CONFIG_USB_DEVICE)
void udc_ctrl(enum usbd_event event, int param)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	switch (event)
	{
		case UDC_CTRL_PULLUP_ENABLE:
			if (param)
				h1940_latch |= (1 << 23);
			else
				h1940_latch &= ~(1 << 23);
			outl(h1940_latch, H1940_LATCH);

			break;
		default:
			break;

	}
}
#endif

void board_video_init(GraphicDevice *pGD)
{
	S3C24X0_LCD * const lcd = S3C24X0_GetBase_LCD();

	lcd->LCDCON1 = 0x00000c78;

	lcd->LCDCON2 = 0x014fc041;
	lcd->LCDCON3 = 0x0098ef09;
	lcd->LCDCON4 = 0x00000009;
	lcd->LCDCON5 = 0x00014f01;
	lcd->LPCSEL  = 0x02;
}

int dram_init (void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

unsigned int dynpart_size[] = {
    0x4000, 0x3c000, 0x4000, 0x8000, 0x300000, 0x1cb4000, 0 };

char *dynpart_names[] = {
    "Boot0", "Boot1", "Env", "Opts", "Kernel", "Filesystem", NULL };

