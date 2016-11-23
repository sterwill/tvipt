/*
 * telnet implementation for busybox
 *
 * Author: Tomi Ollila <too@iki.fi>
 * Copyright (C) 1994-2000 by Tomi Ollila
 *
 * Created: Thu Apr  7 13:29:41 1994 too
 * Last modified: Fri Jun  9 14:34:24 2000 too
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * HISTORY
 * Revision 3.1  1994/04/17  11:31:54  too
 * initial revision
 * Modified 2000/06/13 for inclusion into BusyBox by Erik Andersen <andersen@codepoet.org>
 * Modified 2001/05/07 to add ability to pass TTYPE to remote host by Jim McQuillan
 * <jam@ltsp.org>
 * Modified 2004/02/11 to add ability to pass the USER variable to remote host
 * by Fernando Silveira <swrh@gmx.net>
 *
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "busybox.h"
#include "term.h"

/* from /usr/include/arpa/telnet.h */
#define TELOPT_NEW_ENVIRON 39
#define TELQUAL_IS  0
#define NEW_ENV_VAR 0
#define NEW_ENV_VALUE 1

#define NORETURN 
#define FAST_FUNC inline
typedef signed char smallint;
typedef unsigned char smalluint;

// Fake file descriptors so we can use WiFi101 and Serial 
enum { netfd = 0, termfd = 1 };

/* from include/libbb.h */

enum {
  DATABUFSIZE = 128,
  IACBUFSIZE  = 128,

  CHM_TRY = 0,
  CHM_ON = 1,
  CHM_OFF = 2,

  UF_ECHO = 0x01,
  UF_SGA = 0x02,

  TS_NORMAL = 0,
  TS_COPY = 1,
  TS_IAC = 2,
  TS_OPT = 3,
  TS_SUB1 = 4,
  TS_SUB2 = 5,
  TS_CR = 6,
};

/* from networking/telnet.c */

struct globals {
  int iaclen; /* could even use byte, but it's a loss on x86 */
  byte  telstate; /* telnet negotiation state from network input */
  byte  telwish;  /* DO, DONT, WILL, WONT */
  byte    charmode;
  byte    telflags;
  byte  do_termios;
#if ENABLE_FEATURE_TELNET_TTYPE
  char  *ttype;
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
  const char *autologin;
#endif
#if ENABLE_FEATURE_AUTOWIDTH
  unsigned win_width, win_height;
#endif
  /* buffer to handle telnet negotiations */
  char    iacbuf[IACBUFSIZE];
  WiFiClient * client;
};

struct globals G;

/* Standard handler which just records signo */
smallint bb_got_signal;

/* from libbb/safe_write.c */

ssize_t FAST_FUNC safe_write(int fd, const void *buf, size_t count)
{
  ssize_t n;

  do {
    if (fd == netfd) {
      n = G.client->write((const uint8_t *) buf, count);
    } else {
      n = term_write((const uint8_t *) buf, count);
    }
  } while (n < 0 && errno == EINTR);

  return n;
}

/* from libbb/full_write.c */

/*
 * Write all of the supplied buffer out to a file.
 * This does multiple writes as necessary.
 * Returns the amount written, or -1 on an error.
 */
ssize_t FAST_FUNC full_write(int fd, const void *buf, size_t len)
{
  ssize_t cc;
  ssize_t total;

  total = 0;

  while (len) {
    cc = safe_write(fd, buf, len);

    if (cc < 0) {
      if (total) {
        /* we already wrote some! */
        /* user can do another write to know the error code */
        return total;
      }
      return cc;  /* write() returns -1 on failure. */
    }

    total += cc;
    buf = ((const char *)buf) + cc;
    len -= cc;
  }

  return total;
}

/* from libbb/xfuncs.c */

ssize_t FAST_FUNC full_write1_str(const char *str)
{
  return full_write(termfd, str, strlen(str));
}

/* from networking/telnet.c */

# define IAC   255  /* interpret as command: */
# define DONT  254  /* you are not to use option */
# define DO    253  /* please, you use option */
# define WONT  252  /* I won't use option */
# define WILL  251  /* I will use option */
# define SB    250  /* interpret as subnegotiation */
# define SE    240  /* end sub negotiation */
# define TELOPT_ECHO   1  /* echo */
# define TELOPT_SGA    3  /* suppress go ahead */
# define TELOPT_TTYPE 24  /* terminal type */
# define TELOPT_NAWS  31  /* window size */


void rawmode(void);
void cookmode(void);
void do_linemode(void);
void will_charmode(void);
void telopt(byte c);
void subneg(byte c);

void iac_flush(void)
{
  full_write(netfd, G.iacbuf, G.iaclen);
  G.iaclen = 0;
}

void doexit(int ev)
{
  cookmode();
  G.client->stop();
}

void con_escape(void)
{
  char b;

  if (bb_got_signal) /* came from line mode... go raw */
    rawmode();

  full_write1_str("\r\nConsole escape. Commands are:\r\n"
      " l go to line mode\r\n"
      " c go to character mode\r\n"
      " e exit telnet\r\n");

  if (term_serial.readBytes(&b, 1) == -1)
    doexit(EXIT_FAILURE);

  switch (b) {
  case 'l':
    if (!bb_got_signal) {
      do_linemode();
      goto ret;
    }
    break;
  case 'c':
    if (bb_got_signal) {
      will_charmode();
      goto ret;
    }
    break;
  case 'e':
    doexit(EXIT_SUCCESS);
  }

  full_write1_str("continuing...\r\n");

  if (bb_got_signal)
    cookmode();
 ret:
  bb_got_signal = 0;
}

void busybox_handle_net_output(byte * buf, int len)
{
  byte outbuf[2 * DATABUFSIZE];
  byte *dst = outbuf;
  byte *src = buf;
  byte *end = src + len;

  while (src < end) {
    byte c = *src++;
    if (c == 0x1d) {
      con_escape();
      return;
    }
    *dst = c;
    if (c == IAC)
      *++dst = c; /* IAC -> IAC IAC */
    else
    if (c == '\r' || c == '\n') {
      /* Enter key sends '\r' in raw mode and '\n' in cooked one.
       *
       * See RFC 1123 3.3.1 Telnet End-of-Line Convention.
       * Using CR LF instead of other allowed possibilities
       * like CR NUL - easier to talk to HTTP/SMTP servers.
       */
      *dst = '\r'; /* Enter -> CR LF */
      *++dst = '\n';
    }
    dst++;
  }
  if (dst - outbuf != 0)
    full_write(netfd, outbuf, dst - outbuf);
}


void busybox_handle_net_input(byte * buf, int len)
{
  int i;
  int cstart = 0;

  for (i = 0; i < len; i++) {
    byte c = buf[i];

    if (G.telstate == TS_NORMAL) { /* most typical state */
      if (c == IAC) {
        cstart = i;
        G.telstate = TS_IAC;
      }
      else if (c == '\r') {
        cstart = i + 1;
        G.telstate = TS_CR;
      }
      /* No IACs were seen so far, no need to copy
       * bytes within G.buf: */
      continue;
    }

    switch (G.telstate) {
    case TS_CR:
      /* Prev char was CR. If cur one is NUL, ignore it.
       * See RFC 1123 section 3.3.1 for discussion of telnet EOL handling.
       */
      G.telstate = TS_COPY;
      if (c == '\0')
        break;
      /* else: fall through - need to handle CR IAC ... properly */

    case TS_COPY: /* Prev char was ordinary */
      /* Similar to NORMAL, but in TS_COPY we need to copy bytes */
      if (c == IAC)
        G.telstate = TS_IAC;
      else
        buf[cstart++] = c;
      if (c == '\r')
        G.telstate = TS_CR;
      break;

    case TS_IAC: /* Prev char was IAC */
      if (c == IAC) { /* IAC IAC -> one IAC */
        buf[cstart++] = c;
        G.telstate = TS_COPY;
        break;
      }
      /* else */
      switch (c) {
      case SB:
        G.telstate = TS_SUB1;
        break;
      case DO:
      case DONT:
      case WILL:
      case WONT:
        G.telwish = c;
        G.telstate = TS_OPT;
        break;
      /* DATA MARK must be added later */
      default:
        G.telstate = TS_COPY;
      }
      break;

    case TS_OPT: /* Prev chars were IAC WILL/WONT/DO/DONT */
      telopt(c);
      G.telstate = TS_COPY;
      break;

    case TS_SUB1: /* Subnegotiation */
    case TS_SUB2: /* Subnegotiation */
      subneg(c); /* can change G.telstate */
      break;
    }
  }

  if (G.telstate != TS_NORMAL) {
    /* We had some IACs, or CR */
    if (G.iaclen)
      iac_flush();
    if (G.telstate == TS_COPY) /* we aren't in the middle of IAC */
      G.telstate = TS_NORMAL;
    len = cstart;
  }

  if (len)
    full_write(termfd, buf, len);
}


void put_iac(int c)
{
  G.iacbuf[G.iaclen++] = c;
}

void put_iac2(byte wwdd, byte c)
{
  if (G.iaclen + 3 > IACBUFSIZE)
    iac_flush();

  put_iac(IAC);
  put_iac(wwdd);
  put_iac(c);
}

#if ENABLE_FEATURE_TELNET_TTYPE
void put_iac_subopt(byte c, char *str)
{
  int len = strlen(str) + 6;   // ( 2 + 1 + 1 + strlen + 2 )

  if (G.iaclen + len > IACBUFSIZE)
    iac_flush();

  put_iac(IAC);
  put_iac(SB);
  put_iac(c);
  put_iac(0);

  while (*str)
    put_iac(*str++);

  put_iac(IAC);
  put_iac(SE);
}
#endif

#if ENABLE_FEATURE_TELNET_AUTOLOGIN
void put_iac_subopt_autologin(void)
{
  int len = strlen(G.autologin) + 6;  // (2 + 1 + 1 + strlen + 2)
  const char *p = "USER";

  if (G.iaclen + len > IACBUFSIZE)
    iac_flush();

  put_iac(IAC);
  put_iac(SB);
  put_iac(TELOPT_NEW_ENVIRON);
  put_iac(TELQUAL_IS);
  put_iac(NEW_ENV_VAR);

  while (*p)
    put_iac(*p++);

  put_iac(NEW_ENV_VALUE);

  p = G.autologin;
  while (*p)
    put_iac(*p++);

  put_iac(IAC);
  put_iac(SE);
}
#endif

#if ENABLE_FEATURE_AUTOWIDTH
void put_iac_naws(byte c, int x, int y)
{
  if (G.iaclen + 9 > IACBUFSIZE)
    iac_flush();

  put_iac(IAC);
  put_iac(SB);
  put_iac(c);

  /* "... & 0xff" implicitly done below */
  put_iac(x >> 8);
  put_iac(x);
  put_iac(y >> 8);
  put_iac(y);

  put_iac(IAC);
  put_iac(SE);
}
#endif


void setConMode(void)
{
  if (G.telflags & UF_ECHO) {
    if (G.charmode == CHM_TRY) {
      G.charmode = CHM_ON;
      term_serial.println("telnets: entering character mode");
      term_serial.println("telnets: escape character is '^]'.");
      rawmode();
    }
  } else {
    if (G.charmode != CHM_OFF) {
      G.charmode = CHM_OFF;
      term_serial.println("telnets: entering line mode");
      term_serial.println("telnets: escape character is '^C'.");
      cookmode();
    }
  }
}

void will_charmode(void)
{
  G.charmode = CHM_TRY;
  G.telflags |= (UF_ECHO | UF_SGA);
  setConMode();

  put_iac2(DO, TELOPT_ECHO);
  put_iac2(DO, TELOPT_SGA);
  iac_flush();
}

void do_linemode(void)
{
  G.charmode = CHM_TRY;
  G.telflags &= ~(UF_ECHO | UF_SGA);
  setConMode();

  put_iac2(DONT, TELOPT_ECHO);
  put_iac2(DONT, TELOPT_SGA);
  iac_flush();
}

void to_notsup(char c)
{
  if (G.telwish == WILL)
    put_iac2(DONT, c);
  else if (G.telwish == DO)
    put_iac2(WONT, c);
}

void to_echo(void)
{
  /* if server requests ECHO, don't agree */
  if (G.telwish == DO) {
    put_iac2(WONT, TELOPT_ECHO);
    return;
  }
  if (G.telwish == DONT)
    return;

  if (G.telflags & UF_ECHO) {
    if (G.telwish == WILL)
      return;
  } else if (G.telwish == WONT)
    return;

  if (G.charmode != CHM_OFF)
    G.telflags ^= UF_ECHO;

  if (G.telflags & UF_ECHO)
    put_iac2(DO, TELOPT_ECHO);
  else
    put_iac2(DONT, TELOPT_ECHO);

  setConMode();
  full_write1_str("\r");  /* sudden modec */
}

void to_sga(void)
{
  /* daemon always sends will/wont, client do/dont */

  if (G.telflags & UF_SGA) {
    if (G.telwish == WILL)
      return;
  } else if (G.telwish == WONT)
    return;

  G.telflags ^= UF_SGA; /* toggle */
  if (G.telflags & UF_SGA)
    put_iac2(DO, TELOPT_SGA);
  else
    put_iac2(DONT, TELOPT_SGA);
}

#if ENABLE_FEATURE_TELNET_TTYPE
void to_ttype(void)
{
  /* Tell server we will (or won't) do TTYPE */
  if (G.ttype)
    put_iac2(WILL, TELOPT_TTYPE);
  else
    put_iac2(WONT, TELOPT_TTYPE);
}
#endif

#if ENABLE_FEATURE_TELNET_AUTOLOGIN
void to_new_environ(void)
{
  /* Tell server we will (or will not) do AUTOLOGIN */
  if (G.autologin)
    put_iac2(WILL, TELOPT_NEW_ENVIRON);
  else
    put_iac2(WONT, TELOPT_NEW_ENVIRON);
}
#endif

#if ENABLE_FEATURE_AUTOWIDTH
void to_naws(void)
{
  /* Tell server we will do NAWS */
  put_iac2(WILL, TELOPT_NAWS);
}
#endif

void telopt(byte c)
{
  switch (c) {
  case TELOPT_ECHO:
    to_echo(); break;
  case TELOPT_SGA:
    to_sga(); break;
#if ENABLE_FEATURE_TELNET_TTYPE
  case TELOPT_TTYPE:
    to_ttype(); break;
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
  case TELOPT_NEW_ENVIRON:
    to_new_environ(); break;
#endif
#if ENABLE_FEATURE_AUTOWIDTH
  case TELOPT_NAWS:
    to_naws();
    put_iac_naws(c, G.win_width, G.win_height);
    break;
#endif
  default:
    to_notsup(c);
    break;
  }
}

/* subnegotiation -- ignore all (except TTYPE,NAWS) */
void subneg(byte c)
{
  switch (G.telstate) {
  case TS_SUB1:
    if (c == IAC)
      G.telstate = TS_SUB2;
#if ENABLE_FEATURE_TELNET_TTYPE
    else
    if (c == TELOPT_TTYPE && G.ttype)
      put_iac_subopt(TELOPT_TTYPE, G.ttype);
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
    else
    if (c == TELOPT_NEW_ENVIRON && G.autologin)
      put_iac_subopt_autologin();
#endif
    break;
  case TS_SUB2:
    if (c == SE) {
      G.telstate = TS_COPY;
      return;
    }
    G.telstate = TS_SUB1;
    break;
  }
}

void rawmode(void)
{
  /*
  if (G.do_termios)
    tcsetattr(0, TCSADRAIN, &G.termios_raw);
    */
}

void cookmode(void)
{
  /*
  if (G.do_termios)
    tcsetattr(0, TCSADRAIN, &G.termios_def);
    */
}

void busybox_init(unsigned int win_width, unsigned int win_height, char * ttype, WiFiClient * client, const char * autologin) {
  memset(&G, 0, sizeof(G));
  G.win_width = win_width;
  G.win_height = win_height;
  G.ttype = ttype;
  G.client = client;
  G.autologin = autologin;
}

