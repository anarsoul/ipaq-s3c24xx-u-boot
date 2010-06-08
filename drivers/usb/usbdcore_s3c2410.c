/* S3C2410 USB Device Controller Driver for u-boot
 *
 * (C) Copyright 2007 by OpenMoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 *
 * based on Linux' s3c2410_udc.c, which is
 * Copyright (C) 2004-2006 Herbert Pötzl - Arnaud Patard
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 *
 */

#include <config.h>

#if (defined(CONFIG_S3C2410) || defined(CONFIG_S3C2440) || \
     defined(CONFIG_S3C2442) || defined(CONFIG_S3C2443)) && defined(CONFIG_USB_DEVICE)

#include <common.h>

/* we can't use the regular debug macros since the console might be
 * set to usbtty, which would cause deadlocks! */
#ifdef	DEBUG
#undef debug
#undef debugX
#define debug(fmt,args...)	serial_printf (fmt ,##args)
#define debugX(level,fmt,args...) if (DEBUG>=level) serial_printf(fmt,##args)
#endif

DECLARE_GLOBAL_DATA_PTR;

#include <asm/io.h>
#include <s3c2410.h>

#include "usbdcore.h"
#include "usbdcore_s3c2410.h"
#include "usbdcore_ep0.h"
#include <usb_cdc_acm.h>

static void debug_urb_buffer(char *prefix, struct usb_endpoint_instance *ep)
{
#ifdef DEBUG
	int num;
	static char buf[128];

	if (!ep->tx_urb) {
		serial_printf("no tx_urb\n");
		return;
	}

	num = MIN(ep->tx_urb->actual_length - ep->sent, ep->tx_packetSize);

	memset(buf, 0, sizeof(buf));
	strncpy(buf, ep->tx_urb->buffer + ep->sent, num);

	serial_printf("%s(%d:%s)\n", prefix, num, buf);
#endif
}


enum ep0_state {
        EP0_IDLE,
        EP0_IN_DATA_PHASE,
        EP0_OUT_DATA_PHASE,
        EP0_END_XFER,
        EP0_STALL,
};

static struct urb *ep0_urb = NULL;

static struct usb_device_instance *udc_device;	/* Used in interrupt handler */

static inline int fifo_count_out(void)
{
	int tmp;

	tmp = inl(S3C2410_UDC_OUT_FIFO_CNT2_REG) << 8;
	tmp |= inl(S3C2410_UDC_OUT_FIFO_CNT1_REG);

	return tmp & 0xffff;
}

static const unsigned long ep_fifo_reg[S3C2410_UDC_NUM_ENDPOINTS] = {
	S3C2410_UDC_EP0_FIFO_REG,
	S3C2410_UDC_EP1_FIFO_REG,
	S3C2410_UDC_EP2_FIFO_REG,
	S3C2410_UDC_EP3_FIFO_REG,
	S3C2410_UDC_EP4_FIFO_REG,
};

static int s3c2410_write_noniso_tx_fifo(struct usb_endpoint_instance *endpoint)
{
	struct urb *urb = endpoint->tx_urb;
	unsigned int last, i;
	unsigned int ep = endpoint->endpoint_address & 0x7f;
	unsigned long fifo_reg = ep_fifo_reg[ep];

	/* WARNING: don't ever put serial debug printf's in non-error codepaths
	 * here, it is called from the time critical EP0 codepath ! */

	if (!urb || ep >= S3C2410_UDC_NUM_ENDPOINTS) {
		serial_printf("no urb or wrong endpoint\n");
		return -1;
	}

	S3C2410_UDC_SETIX(ep);
	if ((last = MIN(urb->actual_length - endpoint->sent,
		        endpoint->tx_packetSize))) {
		u8 *cp = urb->buffer + endpoint->sent;

		for (i = 0; i < last; i++)
			outb(*(cp+i), fifo_reg);
	}
	endpoint->last = last;

	if (endpoint->sent + last < urb->actual_length) {
		/* not all data has been transmitted so far */
		return 0;
	}

	if (last == endpoint->tx_packetSize) {
		/* we need to send one more packet (ZLP) */
		return 0;
	}

	return 1;
}


static void s3c2410_deconfigure_device (void)
{
	outl(0, S3C2410_UDC_EP1_DMA_CON);
	outl(0, S3C2410_UDC_EP2_DMA_CON);
	outl(0, S3C2410_UDC_EP3_DMA_CON);
	outl(0, S3C2410_UDC_EP4_DMA_CON);
}

static void s3c2410_configure_device (struct usb_device_instance *device)
{
	S3C24X0_GPIO * const gpio = S3C24X0_GetBase_GPIO();
	S3C24X0_CLOCK_POWER * const cpower = S3C24X0_GetBase_CLOCK_POWER();

	/* Disable UDC DMA */
	outl(0x00, S3C2410_UDC_EP1_DMA_CON);
	outl(0x00, S3C2410_UDC_EP2_DMA_CON);
	outl(0x00, S3C2410_UDC_EP3_DMA_CON);
	outl(0x00, S3C2410_UDC_EP4_DMA_CON);

	outl(0x00, S3C2410_UDC_PWR_REG);

	/* disable EP0-4 SUBD interrupts ? */
	outl(0x00, S3C2410_UDC_USB_INT_EN_REG);

	/* UPLL already configured by board-level init code */

	/* configure USB pads to device mode */
	gpio->MISCCR &= ~(S3C2410_MISCCR_USBHOST | S3C2410_MISCCR_USBSUSPND1 | S3C2410_MISCCR_USBSUSPND0);

	/* don't disable USB clock */
	cpower->CLKSLOW &= ~S3C2410_CLKSLOW_UCLK_OFF;

	/* clear interrupt registers */
	inl(S3C2410_UDC_EP_INT_REG);
	inl(S3C2410_UDC_USB_INT_REG);
	outl(0xff, S3C2410_UDC_EP_INT_REG);
	outl(0xff, S3C2410_UDC_USB_INT_REG);

	/* enable USB interrupts for RESET and SUSPEND/RESUME */
	outl(S3C2410_UDC_USBINT_RESET|S3C2410_UDC_USBINT_SUSPEND,
	     S3C2410_UDC_USB_INT_EN_REG);
}

static void udc_set_address(unsigned char address)
{
	address |= 0x80; /* ADDR_UPDATE bit */
	outl(address, S3C2410_UDC_FUNC_ADDR_REG);
}

extern struct usb_device_descriptor device_descriptor;

static void s3c2410_udc_ep0(void)
{
	u_int8_t ep0csr;
	struct usb_endpoint_instance *ep0 = udc_device->bus->endpoint_array;

	S3C2410_UDC_SETIX(0);
	ep0csr = inl(S3C2410_UDC_IN_CSR1_REG);

	/* clear stall status */
	if (ep0csr & S3C2410_UDC_EP0_CSR_SENTSTL) {
	    	/* serial_printf("Clearing SENT_STALL\n"); */
		clear_ep0_sst();
		if (ep0csr & S3C2410_UDC_EP0_CSR_SOPKTRDY)
			clear_ep0_opr();
		ep0->state = EP0_IDLE;
		return;
	}

	/* clear setup end */
	if (ep0csr & S3C2410_UDC_EP0_CSR_SE
	    /* && ep0->state != EP0_IDLE */) {
	    	/* serial_printf("Clearing SETUP_END\n"); */
		clear_ep0_se();
#if 1
		if (ep0csr & S3C2410_UDC_EP0_CSR_SOPKTRDY) {
			/* Flush FIFO */
			while (inl(S3C2410_UDC_OUT_FIFO_CNT1_REG))
				inl(S3C2410_UDC_EP0_FIFO_REG);
			clear_ep0_opr();
		}
#endif
		ep0->state = EP0_IDLE;
		return;
	}

	/* Don't ever put [serial] debugging in non-error codepaths here, it
	 * will violate the tight timing constraints of this USB Device
	 * controller (and lead to bus enumeration failures) */

	switch (ep0->state) {
		int i, fifo_count;
		unsigned char *datap;
	case EP0_IDLE:
		if (!(ep0csr & S3C2410_UDC_EP0_CSR_OPKRDY))
			break;

		datap = (unsigned char *) &ep0_urb->device_request;
		/* host->device packet has been received */

		/* pull it out of the fifo */
		fifo_count = fifo_count_out();
		for (i = 0; i < fifo_count; i++) {
			*datap = (unsigned char)inl(S3C2410_UDC_EP0_FIFO_REG);
			datap++;
		}
		if (fifo_count != 8) {
			debug("STRANGE FIFO COUNT: %u bytes\n", fifo_count);
			set_ep0_ss();
			return;
		}

		if (ep0_urb->device_request.wLength == 0) {
			if (ep0_recv_setup(ep0_urb)) {
				/* Not a setup packet, stall next EP0 transaction */
				debug("can't parse setup packet1\n");
				set_ep0_ss();
				set_ep0_de_out();
				ep0->state = EP0_IDLE;
				return;
			}
			/* There are some requests with which we need to deal
			 * manually here */
			switch (ep0_urb->device_request.bRequest) {
			case USB_REQ_SET_CONFIGURATION:
				if (!ep0_urb->device_request.wValue)
					usbd_device_event_irq(udc_device,
							DEVICE_DE_CONFIGURED, 0);
				else
					usbd_device_event_irq(udc_device,
							DEVICE_CONFIGURED, 0);
				break;
			case USB_REQ_SET_ADDRESS:
				udc_set_address(udc_device->address);
				usbd_device_event_irq(udc_device,
						DEVICE_ADDRESS_ASSIGNED, 0);
				break;
			default:
				break;
			}
			set_ep0_de_out();
			ep0->state = EP0_IDLE;
		} else {
			if ((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK)
			    == USB_REQ_HOST2DEVICE) {
				clear_ep0_opr();
				ep0->state = EP0_OUT_DATA_PHASE;
				ep0_urb->buffer = ep0_urb->buffer_data;
				ep0_urb->buffer_length = sizeof(ep0_urb->buffer_data);
				ep0_urb->actual_length = 0;
			} else {
				ep0->state = EP0_IN_DATA_PHASE;

				if (ep0_recv_setup(ep0_urb)) {
					/* Not a setup packet, stall next EP0 transaction */
					debug("can't parse setup packet2\n");
					set_ep0_ss();
					//set_ep0_de_out();
					ep0->state = EP0_IDLE;
					return;
				}
				clear_ep0_opr();
				ep0->tx_urb = ep0_urb;
				ep0->sent = ep0->last = 0;

				if (s3c2410_write_noniso_tx_fifo(ep0)) {
					ep0->state = EP0_IDLE;
					set_ep0_de_in();
				} else
					set_ep0_ipr();
			}
		}
		break;
	case EP0_IN_DATA_PHASE:
		if (!(ep0csr & S3C2410_UDC_EP0_CSR_IPKRDY)) {
			ep0->sent += ep0->last;

			if (s3c2410_write_noniso_tx_fifo(ep0)) {
				ep0->state = EP0_IDLE;
				set_ep0_de_in();
			} else
				set_ep0_ipr();
		}
		break;
	case EP0_OUT_DATA_PHASE:
		if (ep0csr & S3C2410_UDC_EP0_CSR_OPKRDY) {
			u32 urb_avail = ep0_urb->buffer_length - ep0_urb->actual_length;
			u_int8_t *cp = ep0_urb->buffer + ep0_urb->actual_length;
			int i, fifo_count;

			fifo_count = fifo_count_out();
			if (fifo_count < urb_avail)
				urb_avail = fifo_count;

			for (i = 0; i < urb_avail; i++)
				*cp++ = inl(S3C2410_UDC_EP0_FIFO_REG);

			ep0_urb->actual_length += urb_avail;

			if (fifo_count < ep0->rcv_packetSize ||
			    ep0_urb->actual_length >= ep0_urb->device_request.wLength) {
				ep0->state = EP0_IDLE;
				if (ep0_recv_setup(ep0_urb)) {
					/* Not a setup packet, stall next EP0 transaction */
					debug("can't parse setup packet3\n");
					set_ep0_ss();
					//set_ep0_de_out();
					return;
				}
				set_ep0_de_out();
			} else
				clear_ep0_opr();
		}
		break;
	case EP0_END_XFER:
		ep0->state = EP0_IDLE;
		break;
	case EP0_STALL:
		//set_ep0_ss;
		ep0->state = EP0_IDLE;
		break;
	}
}


static void s3c2410_udc_epn(int ep)
{
	struct usb_endpoint_instance *endpoint;
	struct urb *urb;
	u32 ep_csr1;

	if (ep >= S3C2410_UDC_NUM_ENDPOINTS)
		return;

	endpoint = &udc_device->bus->endpoint_array[ep];

	S3C2410_UDC_SETIX(ep);

	if (endpoint->endpoint_address & USB_DIR_IN) {
		/* IN transfer (device to host) */
		ep_csr1 = inl(S3C2410_UDC_IN_CSR1_REG);
		debug("for ep=%u, CSR1=0x%x ", ep, ep_csr1);

		urb = endpoint->tx_urb;
		if (ep_csr1 & S3C2410_UDC_ICSR1_SENTSTL) {
			/* Stall handshake */
			debug("stall\n");
			outl(0x00, S3C2410_UDC_IN_CSR1_REG);
			return;
		}
		if (!(ep_csr1 & S3C2410_UDC_ICSR1_PKTRDY) && urb &&
		      urb->actual_length) {

			debug("completing previously send data ");
			usbd_tx_complete(endpoint);

			/* push pending data into FIFO */
			if ((endpoint->last == endpoint->tx_packetSize) &&
			    (urb->actual_length - endpoint->sent - endpoint->last == 0)) {
				endpoint->sent += endpoint->last;
				/* Write 0 bytes of data (ZLP) */
				debug("ZLP ");
				outl(ep_csr1|S3C2410_UDC_ICSR1_PKTRDY, S3C2410_UDC_IN_CSR1_REG);
			} else {
				/* write actual data to fifo */
				debug_urb_buffer("TX_DATA", endpoint);
				s3c2410_write_noniso_tx_fifo(endpoint);
				outl(ep_csr1|S3C2410_UDC_ICSR1_PKTRDY, S3C2410_UDC_IN_CSR1_REG);
			}
		}
		debug("\n");
	} else {
		/* OUT transfer (host to device) */
		ep_csr1 = inl(S3C2410_UDC_OUT_CSR1_REG);
		debug("for ep=%u, CSR1=0x%x ", ep, ep_csr1);

		urb = endpoint->rcv_urb;
		if (ep_csr1 & S3C2410_UDC_OCSR1_SENTSTL) {
			/* Stall handshake */
			outl(0x00, S3C2410_UDC_IN_CSR1_REG);
			return;
		}
		if ((ep_csr1 & S3C2410_UDC_OCSR1_PKTRDY) && urb) {
			/* Read pending data from fifo */
			u32 fifo_count = fifo_count_out();
			int is_last = 0;
			u32 i, urb_avail = urb->buffer_length - urb->actual_length;
			u8 *cp = urb->buffer + urb->actual_length;

			if (fifo_count < endpoint->rcv_packetSize)
				is_last = 1;

			debug("fifo_count=%u is_last=%, urb_avail=%u)\n",
				fifo_count, is_last, urb_avail);

			if (fifo_count < urb_avail)
				urb_avail = fifo_count;

			for (i = 0; i < urb_avail; i++)
				*cp++ = inb(ep_fifo_reg[ep]);

			if (is_last)
				outl(ep_csr1 & ~S3C2410_UDC_OCSR1_PKTRDY,
				     S3C2410_UDC_OUT_CSR1_REG);

			usbd_rcv_complete(endpoint, urb_avail, 0);
		}
	}

	urb = endpoint->rcv_urb;
}

/*
-------------------------------------------------------------------------------
*/

/* this is just an empty wrapper for usbtty who assumes polling operation */
void udc_irq(void)
{
}

/* Handle general USB interrupts and dispatch according to type.
 * This function implements TRM Figure 14-13.
 */
void s3c2410_udc_irq(void)
{
	struct usb_endpoint_instance *ep0 = udc_device->bus->endpoint_array;
	u_int32_t save_idx = inl(S3C2410_UDC_INDEX_REG), idx2;

	/* read interrupt sources */
	u_int32_t usb_status = inl(S3C2410_UDC_USB_INT_REG);
	u_int32_t usbd_status = inl(S3C2410_UDC_EP_INT_REG);
	u_int32_t pwr_reg = inl(S3C2410_UDC_PWR_REG);
	u_int32_t ep0csr = inl(S3C2410_UDC_IN_CSR1_REG);

	//debug("< IRQ usbs=0x%02x, usbds=0x%02x start >", usb_status, usbd_status);

	/* clear interrupts */
	outl(usb_status, S3C2410_UDC_USB_INT_REG);

	if (usb_status & S3C2410_UDC_USBINT_RESET) {
		//serial_putc('R');
		debug("RESET pwr=0x%x\n", inl(S3C2410_UDC_PWR_REG));
		udc_setup_ep(udc_device, 0, ep0);
		outl(S3C2410_UDC_EP0_CSR_SSE|S3C2410_UDC_EP0_CSR_SOPKTRDY, S3C2410_UDC_EP0_CSR_REG);
		ep0->state = EP0_IDLE;
		usbd_device_event_irq (udc_device, DEVICE_RESET, 0);
	}

	if (usb_status & S3C2410_UDC_USBINT_RESUME) {
		debug("RESUME\n");
		usbd_device_event_irq(udc_device, DEVICE_BUS_ACTIVITY, 0);
	}

	if (usb_status & S3C2410_UDC_USBINT_SUSPEND) {
		debug("SUSPEND\n");
		usbd_device_event_irq(udc_device, DEVICE_BUS_INACTIVE, 0);
	}

	/* Endpoint Interrupts */
	if (usbd_status) {
		int i;

		if (usbd_status & S3C2410_UDC_INT_EP0) {
			outl(S3C2410_UDC_INT_EP0, S3C2410_UDC_EP_INT_REG);
			s3c2410_udc_ep0();
		}

		for (i = 1; i < 5; i++) {
			u_int32_t tmp = 1 << i;

			if (usbd_status & tmp) {
				/* FIXME: Handle EP X */
				outl(tmp, S3C2410_UDC_EP_INT_REG);
				s3c2410_udc_epn(i);
			}
		}

		/* what else causes this interrupt? a receive! who is it? */
		if (!usb_status && !usbd_status && !pwr_reg && !ep0csr) {
			for (i = 1; i < 5; i++) {
				idx2 = inl(S3C2410_UDC_INDEX_REG);
				outl(i, S3C2410_UDC_INDEX_REG);

				if (inl(S3C2410_UDC_OUT_CSR1_REG) & 0x1)
					s3c2410_udc_epn(i);

				/* restore index */
				outl(idx2, S3C2410_UDC_INDEX_REG);
			}
		}
	}
	S3C2410_UDC_SETIX(save_idx);
}

/*
-------------------------------------------------------------------------------
*/


/*
 * Start of public functions.
 */

/* Called to start packet transmission. */
void udc_endpoint_write (struct usb_endpoint_instance *endpoint)
{
	unsigned short epnum =
		endpoint->endpoint_address & USB_ENDPOINT_NUMBER_MASK;

	debug("Entering for ep %x ", epnum);

	if (endpoint->tx_urb) {
		u32 ep_csr1;
		debug_urb_buffer("We have an URB, transmitting", endpoint);

		s3c2410_write_noniso_tx_fifo(endpoint);

		S3C2410_UDC_SETIX(epnum);

		ep_csr1 = inl(S3C2410_UDC_IN_CSR1_REG);
		outl(ep_csr1|S3C2410_UDC_ICSR1_PKTRDY, S3C2410_UDC_IN_CSR1_REG);
	} else
		debug("\n");
}

/* Start to initialize h/w stuff */
int udc_init (void)
{
	S3C24X0_CLOCK_POWER * const clk_power = S3C24X0_GetBase_CLOCK_POWER();
	S3C24X0_INTERRUPT * irq = S3C24X0_GetBase_INTERRUPT();

	udc_device = NULL;

	/* Set and check clock control.
	 * We might ought to be using the clock control API to do
	 * this instead of fiddling with the clock registers directly
	 * here.
	 */
	clk_power->CLKCON |= (1 << 7);

	/* Print banner with device revision */
	printf("USB:   S3C2410 USB Deviced\n");

	/*
	 * At this point, device is ready for configuration...
	 */
	outl(0x00, S3C2410_UDC_EP_INT_EN_REG);
	outl(0x00, S3C2410_UDC_USB_INT_EN_REG);

	irq->INTMSK &= ~BIT_USBD;

	return 0;
}

/*
 * udc_setup_ep - setup endpoint
 *
 * Associate a physical endpoint with endpoint_instance
 */
int udc_setup_ep (struct usb_device_instance *device,
		   unsigned int ep, struct usb_endpoint_instance *endpoint)
{
	int ep_addr = endpoint->endpoint_address;
	int packet_size;
	int attributes;
	u_int32_t maxp;

	S3C2410_UDC_SETIX(ep);

	if (ep) {
		if ((ep_addr & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) {
			/* IN endpoint */
			outl(S3C2410_UDC_ICSR1_FFLUSH|S3C2410_UDC_ICSR1_CLRDT,
			     S3C2410_UDC_IN_CSR1_REG);
			outl(S3C2410_UDC_ICSR2_MODEIN, S3C2410_UDC_IN_CSR2_REG);
			packet_size = endpoint->tx_packetSize;
			attributes = endpoint->tx_attributes;
		} else {
			/* OUT endpoint */
			outl(S3C2410_UDC_ICSR1_CLRDT, S3C2410_UDC_IN_CSR1_REG);
			outl(0, S3C2410_UDC_IN_CSR2_REG);
			outl(S3C2410_UDC_OCSR1_FFLUSH|S3C2410_UDC_OCSR1_CLRDT,
			     S3C2410_UDC_OUT_CSR1_REG);
			outl(0, S3C2410_UDC_OUT_CSR2_REG);
			packet_size = endpoint->rcv_packetSize;
			attributes = endpoint->rcv_attributes;
		}
	} else
		packet_size = endpoint->tx_packetSize;

	switch (packet_size) {
	case 8:
		maxp = S3C2410_UDC_MAXP_8;
		break;
	case 16:
		maxp = S3C2410_UDC_MAXP_16;
		break;
	case 32:
		maxp = S3C2410_UDC_MAXP_32;
		break;
	case 64:
		maxp = S3C2410_UDC_MAXP_64;
		break;
	default:
		debug("invalid packet size %u\n", packet_size);
		return -1;
	}

	debug("setting up endpoint %u addr %x packet_size %u maxp %u\n", ep,
		endpoint->endpoint_address, packet_size, maxp);

	/* Set maximum packet size */
	writel(maxp, S3C2410_UDC_MAXP_REG);

	return 0;
}

/* ************************************************************************** */

/**
 * udc_connected - is the USB cable connected
 *
 * Return non-zero if cable is connected.
 */
#if 0
int udc_connected (void)
{
	return ((inw (UDC_DEVSTAT) & UDC_ATT) == UDC_ATT);
}
#endif

/* Turn on the USB connection by enabling the pullup resistor */
void udc_connect (void)
{
	debug("connect, enable Pullup\n");
	S3C24X0_INTERRUPT * irq = S3C24X0_GetBase_INTERRUPT();

	udc_ctrl(UDC_CTRL_PULLUP_ENABLE, 0);
	udelay(10000);
	udc_ctrl(UDC_CTRL_PULLUP_ENABLE, 1);

	irq->INTMSK &= ~BIT_USBD;
}

/* Turn off the USB connection by disabling the pullup resistor */
void udc_disconnect (void)
{
	debug("disconnect, disable Pullup\n");
	S3C24X0_INTERRUPT * irq = S3C24X0_GetBase_INTERRUPT();

	udc_ctrl(UDC_CTRL_PULLUP_ENABLE, 0);

	/* Disable interrupt (we don't want to get interrupts while the kernel
	 * is relocating itself */
	irq->INTMSK |= BIT_USBD;
}

/* Switch on the UDC */
void udc_enable (struct usb_device_instance *device)
{
	debug("enable device %p, status %d\n", device, device->status);

	/* Save the device structure pointer */
	udc_device = device;

	/* Setup ep0 urb */
	if (!ep0_urb)
		ep0_urb = usbd_alloc_urb(udc_device,
					 udc_device->bus->endpoint_array);
	else
		serial_printf("udc_enable: ep0_urb already allocated %p\n",
			       ep0_urb);

	s3c2410_configure_device(device);
}

/* Switch off the UDC */
void udc_disable (void)
{
	debug("disable UDC\n");

	s3c2410_deconfigure_device();

	/* Free ep0 URB */
	if (ep0_urb) {
		/*usbd_dealloc_urb(ep0_urb); */
		ep0_urb = NULL;
	}

	/* Reset device pointer.
	 * We ought to do this here to balance the initialization of udc_device
	 * in udc_enable, but some of our other exported functions get called
	 * by the bus interface driver after udc_disable, so we have to hang on
	 * to the device pointer to avoid a null pointer dereference. */
	/* udc_device = NULL; */
}

/**
 * udc_startup - allow udc code to do any additional startup
 */
void udc_startup_events (struct usb_device_instance *device)
{
	/* The DEVICE_INIT event puts the USB device in the state STATE_INIT. */
	usbd_device_event_irq (device, DEVICE_INIT, 0);

	/* The DEVICE_CREATE event puts the USB device in the state
	 * STATE_ATTACHED.
	 */
	usbd_device_event_irq (device, DEVICE_CREATE, 0);

	/* Some USB controller driver implementations signal
	 * DEVICE_HUB_CONFIGURED and DEVICE_RESET events here.
	 * DEVICE_HUB_CONFIGURED causes a transition to the state STATE_POWERED,
	 * and DEVICE_RESET causes a transition to the state STATE_DEFAULT.
	 * The OMAP USB client controller has the capability to detect when the
	 * USB cable is connected to a powered USB bus via the ATT bit in the
	 * DEVSTAT register, so we will defer the DEVICE_HUB_CONFIGURED and
	 * DEVICE_RESET events until later.
	 */

	/* The GTA01 can detect usb device attachment, but we just assume being
	 * attached for now (go to STATE_POWERED) */
	usbd_device_event_irq (device, DEVICE_HUB_CONFIGURED, 0);

	udc_enable (device);
}

void udc_set_nak(int epid)
{
	/* FIXME: implement this */
}

void udc_unset_nak(int epid)
{
	/* FIXME: implement this */
}

#endif /* CONFIG_S3C2410 && CONFIG_USB_DEVICE */
