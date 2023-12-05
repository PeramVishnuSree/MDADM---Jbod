#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  int n = 0;
  while (n < len) {
    int r = read(fd, &buf[n], (len - n));
    if (r < 0) {
      return false;
    }
    n +=r;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
  int n = 0;
  while (n < len) {
    int w = write(fd, &buf[n], (len-n));
    if (w < 0) {
      return false;
    }
    n +=w;
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {

  uint8_t mybuf[HEADER_LEN];
  uint16_t len;
  nread(fd, HEADER_LEN, mybuf);

  memcpy(&len, mybuf, 2);
  memcpy(op, mybuf + 2, 4);
  memcpy(ret, mybuf + 6, 2);

  len = ntohs(len);
  *op = ntohl(*op);
  *ret = ntohs(*ret);

  if (len == HEADER_LEN) {
    return true;
  }

  if (len == (256 + 8)) {
    nread(fd, 256, block);
    return true;
  }

  return false;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {

  uint16_t len = HEADER_LEN;
  uint32_t command = op >> 26;

  op = htonl(op);

  if(command == JBOD_WRITE_BLOCK) {
    uint8_t packet[HEADER_LEN + JBOD_BLOCK_SIZE];
    len += JBOD_BLOCK_SIZE;
    len = htons(len);
    
    memcpy(packet, &len, 2);
    memcpy(packet + 2, &op, 4);
    memcpy(packet + 8, block, 256);
    nwrite(sd, 264, packet);
  }

  else{
    uint8_t packet[HEADER_LEN];
    len = htons(len);
    memcpy(packet, &len, 2);
    memcpy(packet + 2, &op, 4);
    nwrite(sd, 8, packet);
  }

  return true;

}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {

  cli_sd = socket(PF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1) {
    printf("Error on socket creation [%s]\n", strerror(errno));
    return (-1);
  }

  // setting up address infromation
  struct sockaddr_in v4;
  v4.sin_family = AF_INET;
  v4.sin_port = htons(port);
  if (inet_aton(ip, &v4.sin_addr) == 0){
    return ( false );
  }

  // connecting
  if ( connect(cli_sd, (const struct sockaddr *) &v4, sizeof(v4)) == -1) {
    printf("problem establishing connection");
    return (false);
  }

  return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  /* close the connection*/
  close( cli_sd );
  cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint16_t ret;
  send_packet(cli_sd, op, block);
  recv_packet(cli_sd, &op, &ret, block);
  return 0;

}
