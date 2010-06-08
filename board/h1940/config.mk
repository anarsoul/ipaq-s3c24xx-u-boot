#
# (C) Copyright 2002
# Gary Jennejohn, DENX Software Engineering, <gj@denx.de>
# David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
#
# SAMSUNG SMDK2410 board with S3C2410X (ARM920T) cpu
#
# see http://www.samsung.com/ for more information on SAMSUNG
#

CONFIG_USB_DFU_VENDOR=0x1457
CONFIG_USB_DFU_PRODUCT=0x511d
CONFIG_USB_DFU_REVISION=0x0100

#
# h1940 has 1 bank of 64 MB DRAM
#
# 3000'0000 to 3400'0000
#
# Linux-Kernel is expected to be at 3010'8000, entry 3010'8000
# optionally with a ramdisk at 3090'0000
#
# we load ourself to 33F8'0000
#
# download area is 3100'0000
#


TEXT_BASE = 0x33F80000
