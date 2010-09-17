/*
 * bootmenu_simple.c - Simplified boot menu
 *
 * Copyright (C) 2010 Vasily Khoruzhick <anarsoul@gmail.com>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <common.h>

#ifdef CFG_BOOTMENU_SIMPLE

#include <bootmenu_simple.h>
#include <console.h>

#define DEBOUNCE_LOOPS		1000	/* wild guess */

struct bootmenu_simple_setup *setup;
static int bootindex;

static int debounce(int (*fn)(void), int *last)
{
	int on, i;

again:
	on = fn();
	if (on != *last)
		for (i = DEBOUNCE_LOOPS; i; i--)
			if (on != fn())
				goto again;
	*last = on;
	return on;
}

static int next_key(void)
{
	static int last_next = -1;

	return debounce(setup->next_key, &last_next);
}

static int prev_key(void)
{
	static int last_prev = -1;

	return debounce(setup->prev_key, &last_prev);
}

static int select_key(void)
{
	static int last_select = -1;

	return debounce(setup->select_key, &last_select);
}

static void bootmenu_simple_hook(int activity)
{
	int old_bootindex = bootindex;
	char bootindex_str[] = "0";
	if (prev_key() && bootindex > 0) {
		while (prev_key());
		bootindex--;
	}

	if (next_key() && bootindex < 9) {
		while (next_key());
		bootindex++;
	}

	if (select_key())
		run_command(getenv("bootcmd"), 0);

	if (old_bootindex != bootindex) {
		bootindex_str[0] = bootindex + '0';
		setup->print_index(bootindex);
		setenv("bootindex", bootindex_str);
	}
}

void bootmenu_simple_init(struct bootmenu_simple_setup *user_setup)
{
	char *bootindex_str;
	setup = user_setup;

	bootindex_str = getenv("bootindex");
	if (bootindex_str && strlen(bootindex_str) >= 1 &&
		bootindex_str[0] >= '0' && bootindex_str[0] <= '9')
		bootindex = bootindex_str[0] - '0';

	setup->print_index(bootindex);

	console_poll_hook = bootmenu_simple_hook;
}

#endif
