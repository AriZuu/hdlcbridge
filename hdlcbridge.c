/*
 * Copyright (c) 2014, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>

#ifdef linux
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

#include <termios.h>

#include "ppp_defs.h"
#include "ppp_frame.h"

int tapOpen(char* ip)
{
  int tap;
  char buf[80];

#ifdef linux

  tap = open("/dev/net/tun", O_RDWR);
  if (tap == -1) {

    perror("tap");
    return -1;
  }

  struct ifreq ifr;

  memset(&ifr, '\0', sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strcpy(ifr.ifr_name, "tap0");

  if (ioctl(tap, TUNSETIFF, &ifr) == -1) {

    perror("ioctl TUNSETIFF");
    close(tap);
    return -1;
  }

  printf("TAP is %s\n", ifr.ifr_name);
  if (ip) {

    sprintf(buf, "ifconfig tap0 inet %s up", ip);
    system(buf);
  }
  else
    system("ifconfig tap0 up");

#else

  tap = open("/dev/tap0", O_RDWR);
  if (tap == -1) {
  
    perror("tap");
    return -1;
  }

  printf("TAP is %s\n", fdevname(tap));

  if (ip) {

    sprintf(buf, "ifconfig tap0 inet %s", ip);
    system(buf);
    system("ifconfig tap0 inet6 -ifdisabled up");
  }
    system("ifconfig tap0 up");

#endif

  system("ifconfig tap0");

  return tap;
}

int serverOpen(bool ipv6)
{
  int lsn;
  int flag;
  struct sockaddr_in me4;
  struct sockaddr_in6 me;

  if (ipv6) {

    memset(&me, '\0', sizeof(me));
    me.sin6_family = AF_INET6;
    me.sin6_addr = in6addr_any;
    me.sin6_port = htons(33333);

    lsn = socket(AF_INET6, SOCK_STREAM, 0);
  }
  else {
  
    memset(&me, '\0', sizeof(me));
    me4.sin_family = AF_INET;
    me4.sin_addr.s_addr = INADDR_ANY;
    me4.sin_port = htons(33333);

    lsn = socket(AF_INET, SOCK_STREAM, 0);
  }

  if (lsn == -1) {

    perror("socket");
    return -1;
  }
  
  flag = 1;
  if (setsockopt(lsn, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1) {

    perror("SO_REUSEADDR");
    return -1;
  }

  if (ipv6) {

    flag = 0;
    if (setsockopt(lsn, IPPROTO_IPV6, IPV6_V6ONLY, &flag, sizeof(int)) == -1) {

      perror("IPV6_V6ONLY");
      return -1;
    }

    if (bind(lsn, (struct sockaddr*)&me, sizeof(me)) == -1) {
 
      perror("bind");
      return -1;
    }
  }
  else {

    if (bind(lsn, (struct sockaddr*)&me4, sizeof(me4)) == -1) {
 
      perror("bind");
      return -1;
    }
  }

  if (listen(lsn, 5) == -1) {

    perror("listen");
    return -1;
  }

  return lsn;
}

void sendDebug(int fd, char* msg)
{
  PPPContext ctx;
  uint8_t pkt[1024];
  int  i;
  int  len;

  ctx.buf = pkt;
  pppOutputBegin(&ctx, PPP_DEBUG);
  
  len = strlen(msg);
  for (i = 0; i < len; i++)
    pppOutputAppend(&ctx, *msg++);

  pppOutputEnd(&ctx);

  len = ctx.ptr - ctx.buf;
  write(fd, ctx.buf, len);
}

int clientConnect(char* host)
{
  struct addrinfo hints;
  struct addrinfo *res, *resOrig;
  int    cli;
  int    i;

  memset(&hints, '\0', sizeof(hints));

  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  i = getaddrinfo(host, "33333", &hints, &res);
  if (i != 0) {

    fprintf(stderr, "host %s: %s\n", host, gai_strerror(i));
    return -1;
  }

  resOrig = res;
  cli = -1;

  while (res) {

    cli = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (cli >= 0) {

      if (connect(cli, res->ai_addr, res->ai_addrlen) == 0)
        break;

      close(cli);
      cli = -1;
    }

    res = res->ai_next;
  }

  if (cli == -1) 
    perror("connect");

  freeaddrinfo(resOrig);
 
  if (cli != -1) {

    char buf[1024];

    sprintf(buf, "hdlcbridge client here.\n");
    sendDebug(cli, buf);
  }

  return cli;
}

int clientAccept(int lsn)
{
  int s;
  socklen_t addrlen;

  struct sockaddr_in6 peer;

  s = accept(lsn, (struct sockaddr*)&peer, &addrlen);
  if (s == -1) {

    perror("accept");
    return -1;
  }


  char buf[1024];

  sprintf(buf, "hdlcbridge server accepted you.\n");
  sendDebug(s, buf);

  return s;
}


PPPContext clientCtx;
uint8_t clientBuf[2000];
int globalTapXXX;

int clientRead(int client, int tap)
{
  int len;
  uint8_t buf[2000];
  uint8_t *ptr;
  int i;

  len = read(client, buf, sizeof(buf));
  if (len == -1) {

    perror("client read");
    return -1;
  }

  if (len == 0)
    return -1;

  ptr = buf;
  for (i = 0; i < len; i++)
    pppInputAppend(&clientCtx, *ptr++);

  return len;
}

void tapWrite(int proto, uint8_t* data, int len)
{
  if (proto == PPP_ETHERNET) {

    if (write(globalTapXXX, data, len) != len)
      perror("tap write");
  }
  else if (proto == PPP_DEBUG) {

    data[len] = '\0';
    printf ("%s", data);
  }
}

int tapRead(int tap, int client)
{
  int len;
  uint8_t buf[2000];

  len = read(tap, buf, sizeof(buf));
  if (len == -1) {

    perror("tap read");
    return -1;
  }

  if (len == 0)
    return -1;

  uint8_t buf2[10000];
  int i;

  PPPContext ctx;

  ctx.buf = buf2;
  pppOutputBegin(&ctx, PPP_ETHERNET);
  for (i = 0; i < len; i++)
    pppOutputAppend(&ctx, buf[i]);

  pppOutputEnd(&ctx);

  if (client != -1) {

    int l = ctx.ptr - ctx.buf;
    write(client, ctx.buf, l);
  }

  return len;
}

int serialOpen(char* port)
{
  int fd;
  struct termios tty;

  fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd == -1) {

    perror(port);
    return -1;
  }

  tcgetattr(fd, &tty);
  cfmakeraw(&tty);

  tty.c_cflag |= CLOCAL;
  cfsetspeed(&tty, B115200);
  if (tcsetattr(fd, TCSAFLUSH, &tty) < 0) {

    perror("tcsetattr");
    close(fd);
    return -1;
  }
  
  return fd;
}

static struct option longopts[] = {
  { "server",   no_argument,            NULL,           's' },
  { "client",   required_argument,      NULL,           'c' },
  { "serial",   required_argument,      NULL,           'p' },
  { "ifconfig", required_argument,      NULL,           'i' },
  { "ipv6",     no_argument,            NULL,           '6' },
  { NULL,       0,                      NULL,           0 }
};


int main(int argc, char** argv)
{
  int ch;
  char* clientMode = NULL;
  char* serialMode = NULL;
  bool  serverMode = false;
  bool  ipv6 = false;
  char* ifconfig = NULL;
  
  while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {

    switch (ch) {
    case 's':
      serverMode = true;
      break;

    case '6':
      ipv6 = true;
      break;

    case 'p':
      serialMode = optarg;
      break;

    case 'i':
      ifconfig = optarg;
      break;

    case 'c':
      clientMode = optarg;
      break;

    default:
      fprintf(stderr, "wrong opt\n");
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if ((clientMode != NULL ? 1 : 0) +
      (serverMode ? 1 : 0) +
      (serialMode ? 1 : 0) != 1) {

    fprintf(stderr, "Exactly one of --server, --client or --serial must be specified.\n");
    exit(1);
  }

  int tap = -1;
  int server = -1;
  int client = -1;

  clientCtx.buf = clientBuf;
  clientCtx.inputHook = tapWrite;
  pppInputBegin(&clientCtx);

  if (serverMode) {

    server = serverOpen(ipv6);
    if (server == -1)
      exit(1);
  }

  if (clientMode) {

    client = clientConnect(clientMode);
    if (client == -1)
      exit(1);
  }

  if (serialMode) {

    client = serialOpen(serialMode);
    if (client == -1)
      exit(1);
  }

  tap = tapOpen(ifconfig);
  if (tap == -1)
    exit(1);

  globalTapXXX = tap;

  while (true) {

    int maxFd = -1;
    fd_set rdSet;
    int status;

    FD_ZERO(&rdSet);
    if (tap != -1) {

      FD_SET(tap, &rdSet);
      if (tap > maxFd)
        maxFd = tap;
    }

    if (server != -1) {

      FD_SET(server, &rdSet);
      if (server > maxFd)
        maxFd = server;
    }

    if (client != -1) {

      FD_SET(client, &rdSet);
      if (client > maxFd)
        maxFd = client;
    }

    status = select(maxFd + 1, &rdSet, NULL, NULL, NULL);
    if (status == -1)  {

      perror("select");
      exit(1);
    }

    if (server != -1 && FD_ISSET(server, &rdSet)) {

      if (client != -1)
        close(client);

      client = clientAccept(server);
      if (client == -1)
        exit(1);
    }

    if (client != -1 && FD_ISSET(client, &rdSet)) {

      if (clientRead(client, tap) == -1) {

        close(client);
        client = -1;
      }
    }

    if (tap != -1 && FD_ISSET(tap, &rdSet)) {

      if (tapRead(tap, client) == -1) {

        exit(1);
      }
    }
  }
}
