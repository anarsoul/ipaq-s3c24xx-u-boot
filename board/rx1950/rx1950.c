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
#include <s3c2440.h>

DECLARE_GLOBAL_DATA_PTR;

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
void rx1950_led_set(int value)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	value += 1;

	if (value & 1)
		gpio->GPADAT |= (1 << 6);
	else
		gpio->GPADAT &= ~(1 << 6);

	if (value & 2)
		gpio->GPADAT |= (1 << 7);
	else
		gpio->GPADAT &= ~(1 << 7);

	if (value & 4)
		gpio->GPADAT |= (1 << 11);
	else
		gpio->GPADAT &= ~(1 << 11);
}

int rx1950_up_pressed(void)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	if (gpio->GPGDAT & (1 << 4))
		return 0;
	return 1;
}

int rx1950_down_pressed(void)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	if (gpio->GPGDAT & (1 << 6))
		return 0;
	return 1;
}

static struct bootmenu_simple_setup rx1950_bm_setup = {
	.next_key = rx1950_down_pressed,
	.prev_key = rx1950_up_pressed,
	.print_index = rx1950_led_set,
};

/*
 * Miscellaneous platform dependent initialisations
 */

int board_init (void)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();

	/* Configure some GPIO banks */
	gpio->GPECON = 0xa56aa955;
	gpio->GPJCON = 0x01555555;

	/* Enable mmc power */
	gpio->GPJDAT |= (1 << 1);

	/* Disable write protect */
	gpio->GPADAT |= (1 << 0);

	/* arch number of SMDK2410-Board */
	gd->bd->bi_arch_number = MACH_TYPE_RX1950;

	/* adress of boot parameters */
	gd->bd->bi_boot_params = 0x30100100;

	icache_enable();
	dcache_enable();

	bootmenu_simple_init(&rx1950_bm_setup);

	return 0;
}

int board_late_init(void)
{
	bootmenu_simple_init(&rx1950_bm_setup);

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
				gpio->GPJDAT |= (1 << 5);
			else
				gpio->GPJDAT &= ~(1 << 5);
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
    0x4000, 0x40000, 0x300000, 0x3cbc000, 0 };

char *dynpart_names[] = {
    "Boot0", "Boot1", "Kernel", "Filesystem", NULL };

