/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 * Copyright (C) 2009 Lars Immisch
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* $Id$ */

/*
 * avrdude interface for Arduino programmer
 *
 * The Arduino programmer is mostly a STK500v1, just the signature bytes
 * are read differently.
 */

#include "ac_cfg.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <wiringPi.h>

#include "avrdude.h"
#include "pgm.h"
#include "stk500_private.h"
#include "stk500.h"
#include "serial.h"

// Value obtained by $ gpio readall (value of wPi)
// This value is neither the GPIO or BCM num!
// DTR pulls to ground while we'll set it to 3v3. ACCOUNT FOR THAT!
#define DTR_ALT_PIN 28

/* read signature bytes - arduino version */
static int arduino_read_sig_bytes(PROGRAMMER * pgm, AVRPART * p, AVRMEM * m)
{
  unsigned char buf[32];

  /* Signature byte reads are always 3 bytes. */

  if (m->size < 3) {
    fprintf(stderr, "%s: memsize too small for sig byte read", progname);
    return -1;
  }

  buf[0] = Cmnd_STK_READ_SIGN;
  buf[1] = Sync_CRC_EOP;

  serial_send(&pgm->fd, buf, 2);

  if (serial_recv(&pgm->fd, buf, 5) < 0)
    return -1;
  if (buf[0] == Resp_STK_NOSYNC) {
    fprintf(stderr, "%s: stk500_cmd(): programmer is out of sync\n",
			progname);
	return -1;
  } else if (buf[0] != Resp_STK_INSYNC) {
    fprintf(stderr,
			"\n%s: arduino_read_sig_bytes(): (a) protocol error, "
			"expect=0x%02x, resp=0x%02x\n",
			progname, Resp_STK_INSYNC, buf[0]);
	return -2;
  }
  if (buf[4] != Resp_STK_OK) {
    fprintf(stderr,
			"\n%s: arduino_read_sig_bytes(): (a) protocol error, "
			"expect=0x%02x, resp=0x%02x\n",
			progname, Resp_STK_OK, buf[4]);
    return -3;
  }

  m->buf[0] = buf[1];
  m->buf[1] = buf[2];
  m->buf[2] = buf[3];

  return 3;
}

static int arduino_open(PROGRAMMER * pgm, char * port)
{
  strcpy(pgm->port, port);
  if (serial_open(port, pgm->baudrate? pgm->baudrate: 115200, &pgm->fd)==-1) {
    return -1;
  }

  /* Clear DTR and RTS to unload the RESET capacitor 
   * (for example in Arduino) */
  //serial_set_dtr_rts(&pgm->fd, 0);
  digitalWrite(DTR_ALT_PIN, 0);
  usleep(50*1000);
  /* Set DTR and RTS back to high */
  //serial_set_dtr_rts(&pgm->fd, 1);
  digitalWrite(DTR_ALT_PIN, 1);
  usleep(50*1000);

  /*
   * drain any extraneous input
   */
  stk500_drain(pgm, 0);

  if (stk500_getsync(pgm) < 0)
    return -1;

  return 0;
}

static void arduino_close(PROGRAMMER * pgm)
{
  //serial_set_dtr_rts(&pgm->fd, 0);
  digitalWrite(DTR_ALT_PIN, 0);
  serial_close(&pgm->fd);
  pgm->fd.ifd = -1;
}

void arduino_initpgm(PROGRAMMER * pgm)
{
	/* This is mostly a STK500; just the signature is read
     differently than on real STK500v1 
     and the DTR signal is set when opening the serial port
     for the Auto-Reset feature */
  stk500_initpgm(pgm);

  strcpy(pgm->type, "Arduino");
  pgm->read_sig_bytes = arduino_read_sig_bytes;
  pgm->open = arduino_open;
  pgm->close = arduino_close;

  wiringPiSetup();
  pinMode(DTR_ALT_PIN, OUTPUT);
  digitalWrite(DTR_ALT_PIN, 1); // DTR is true by default
}
