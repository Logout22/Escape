/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <esc/common.h>
#include <esc/ports.h>
#include <esc/io.h>
#include <esc/signals.h>
#include <esc/driver.h>
#include <esc/heap.h>
#include <esc/fileio.h>
#include <messages.h>
#include <esc/lock.h>
#include <ringbuffer.h>
#include <errors.h>

/* FIXME: THIS MAY CAUSE TROUBLE SINCE WE'RE USING AN IO-PORT THAT IS USED BY THE
 * KEYBOARD-DRIVER, TOO */
/* we need some kind of locking-mechanism here to fix it.. */

/* io ports */
#define IOPORT_KB_CTRL				0x64
#define IOPORT_KB_DATA				0x60
#define IOPORT_KB_INOUTBUF			0x60

#define KBC_CMD_READ_STATUS			0x20
#define KBC_CMD_SET_STATUS			0x60
#define KBC_CMD_DISABLE_MOUSE		0xA7
#define KBC_CMD_ENABLE_MOUSE		0xA8
#define KBC_CMD_DISABLE_KEYBOARD	0xAD
#define KBC_CMD_ENABLE_KEYBOARD		0xAE
#define KBC_CMD_NEXT2MOUSE			0xD4
#define MOUSE_CMD_STREAMING			0xF4

/* bits of the status-byte */
#define KBC_STATUS_DATA_AVAIL		(1 << 0)
#define KBC_STATUS_BUSY				(1 << 1)
#define KBC_STATUS_SELFTEST_OK		(1 << 2)
#define KBC_STATUS_LAST_WAS_CMD		(1 << 3)
#define KBC_STATUS_KB_LOCKED		(1 << 4)
#define KBC_STATUS_MOUSE_DATA_AVAIL	(1 << 5)
#define KBC_STATUS_TIMEOUT			(1 << 6)
#define KBC_STATUS_PARITY_ERROR		(1 << 7)

/* some bits in the command-byte */
#define KBC_CMDBYTE_TRANSPSAUX		(1 << 6)
#define KBC_CMDBYTE_DISABLE_KB		(1 << 4)
#define KBC_CMDBYTE_ENABLE_IRQ12	(1 << 1)
#define KBC_CMDBYTE_ENABLE_IRQ1		(1 << 0)

#define KB_MOUSE_LOCK				0x1

#define INPUT_BUF_SIZE				128

static void irqHandler(tSig sig,u32 data);
static void kb_init(void);
static void kb_checkCmd(void);
static u16 kb_read(void);
static u16 kb_readMouse(void);
static u16 kb_writeMouse(u8 cmd);

/* a mouse-packet */
typedef struct {
	union {
		u8	yOverflow : 1,
			xOverflow : 1,
			ySign : 1,
			xSign : 1,
			: 1,
			middleBtn : 1,
			rightBtn : 1,
			leftBtn : 1;
		u8 all;
	} status;
	s8 xcoord;
	s8 ycoord;
} sMousePacket;

static u8 byteNo = 0;
static tDrvId sid;
static sMsg msg;
static sRingBuf *ibuf;
static sRingBuf *rbuf;
static sMouseData mdata;
static sMousePacket packet;
static bool moving = false;

int main(void) {
	tMsgId mid;

	/* request io-ports */
	if(requestIOPort(IOPORT_KB_CTRL) < 0 || requestIOPort(IOPORT_KB_DATA) < 0)
		error("Unable to request io-ports");

	kb_init();

	/* create input-buffer */
	ibuf = rb_create(sizeof(sMouseData),INPUT_BUF_SIZE,RB_OVERWRITE);
	rbuf = rb_create(sizeof(sMouseData),INPUT_BUF_SIZE,RB_OVERWRITE);
	if(ibuf == NULL || rbuf == NULL)
		error("Unable to create ring-buffers");

	/* reg intrpt-handler */
	if(setSigHandler(SIG_INTRPT_MOUSE,irqHandler) < 0)
		error("Unable to announce interrupt-handler");

	/* reg driver */
	sid = regDriver("mouse",DRV_READ);
	if(sid < 0)
		error("Unable to register driver '%s'","mouse");

    /* wait for commands */
	while(1) {
		tFD fd;

		/* move mouse-packages (we can't access ibuf while doing this) */
		moving = true;
		rb_move(rbuf,ibuf,rb_length(ibuf));
		if(rb_length(rbuf) > 0)
			setDataReadable(sid,true);
		moving = false;

		fd = getWork(&sid,1,NULL,&mid,&msg,sizeof(msg),0);
		if(fd < 0) {
			if(fd != ERR_INTERRUPTED)
				printe("[MOUSE] Unable to get work");
		}
		else {
			switch(mid) {
				case MSG_DRV_READ: {
					/* offset is ignored here */
					u32 count = msg.args.arg2 / sizeof(sMouseData);
					sMouseData *buffer = (sMouseData*)malloc(count * sizeof(sMouseData));
					msg.args.arg1 = 0;
					if(buffer)
						msg.args.arg1 = rb_readn(rbuf,buffer,count) * sizeof(sMouseData);
					msg.args.arg2 = rb_length(rbuf) > 0;
					send(fd,MSG_DRV_READ_RESP,&msg,sizeof(msg.args));
					if(buffer) {
						send(fd,MSG_DRV_READ_RESP,buffer,count * sizeof(sMouseData));
						free(buffer);
					}
				}
				break;
			}
			close(fd);
		}
	}

	/* cleanup */
	rb_destroy(ibuf);
	rb_destroy(rbuf);
	releaseIOPort(IOPORT_KB_CTRL);
	releaseIOPort(IOPORT_KB_DATA);
	unsetSigHandler(SIG_INTRPT_MOUSE);
	unregDriver(sid);
	return EXIT_SUCCESS;
}

static void irqHandler(tSig sig,u32 data) {
	u8 status;
	UNUSED(sig);
	UNUSED(data);

	/* check if there is mouse-data */
	status = inByte(IOPORT_KB_CTRL);
	if(!(status & KBC_STATUS_MOUSE_DATA_AVAIL))
		return;

	switch(byteNo) {
		case 0:
			packet.status.all = inByte(IOPORT_KB_DATA);
			byteNo++;
			break;
		case 1:
			packet.xcoord = inByte(IOPORT_KB_DATA);
			byteNo++;
			break;
		case 2:
			packet.ycoord = inByte(IOPORT_KB_DATA);
			byteNo = 0;
			/* if we're currently moving stuff from ibuf to rbuf, we can't access ibuf */
			/* so, simply skip the packet in this case */
			if(!moving) {
				/* write the message in our ringbuffer */
				mdata.x = packet.xcoord;
				mdata.y = packet.ycoord;
				mdata.buttons = (packet.status.leftBtn << 2) |
					(packet.status.rightBtn << 1) |
					(packet.status.middleBtn << 0);
				rb_write(ibuf,&mdata);
			}
			break;
	}
}

static void kb_init(void) {
	u8 cmdByte;
	/* activate mouse */
	outByte(IOPORT_KB_CTRL,KBC_CMD_ENABLE_MOUSE);
	kb_checkCmd();

	/* put mouse in streaming mode */
	kb_writeMouse(MOUSE_CMD_STREAMING);

	/* read cmd byte */
	outByte(IOPORT_KB_CTRL,KBC_CMD_READ_STATUS);
	kb_checkCmd();
	cmdByte = kb_read();
	outByte(IOPORT_KB_CTRL,KBC_CMD_SET_STATUS);
	kb_checkCmd();
	/* somehow cmdByte is 0 in vbox and qemu. we have to enable TRANSPSAUX in this case.
	 * otherwise we get strange scancodes in the keyboard-driver */
	cmdByte |= KBC_CMDBYTE_TRANSPSAUX | KBC_CMDBYTE_ENABLE_IRQ12 | KBC_CMDBYTE_ENABLE_IRQ1;
	cmdByte &= ~KBC_CMDBYTE_DISABLE_KB;
	outByte(IOPORT_KB_DATA,cmdByte);
	kb_checkCmd();
}

static void kb_checkCmd(void) {
	while(inByte(IOPORT_KB_CTRL) & KBC_STATUS_BUSY);
}

static u16 kb_read(void) {
	u16 c = 0;
	u8 status;
	while(c++ < 0xFFFF && !((status = inByte(IOPORT_KB_CTRL)) & KBC_STATUS_DATA_AVAIL));
	if(!(status & KBC_STATUS_DATA_AVAIL))
		return 0xFF00;
	return inByte(IOPORT_KB_DATA);
}

static u16 kb_readMouse(void) {
	u16 c = 0;
	u8 status;
	while(c++ < 0xFFFF && !((status = inByte(IOPORT_KB_CTRL)) & KBC_STATUS_MOUSE_DATA_AVAIL));
	if(!(status & KBC_STATUS_MOUSE_DATA_AVAIL))
		return 0xFF00;
	return inByte(IOPORT_KB_DATA);
}

static u16 kb_writeMouse(u8 cmd) {
	outByte(IOPORT_KB_CTRL,KBC_CMD_NEXT2MOUSE);
	kb_checkCmd();
	outByte(IOPORT_KB_DATA,cmd);
	kb_checkCmd();
	return kb_read();
}