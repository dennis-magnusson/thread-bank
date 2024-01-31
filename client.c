#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define BUFSIZE 255
#define SOCKET_ADDR "./socket"

void copydata(int from, int to) {
  char buf[1024];
  int amount;

  while ((amount = read(from, buf, sizeof(buf))) > 0) {
    assert((write(to, buf, amount) == amount));
  }
  assert(amount >= 0);
}


int connect_to_server(void) {
  struct sockaddr_un address;
  int sock;
  size_t addrLength;

  assert((sock = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);

  address.sun_family = AF_UNIX;    /* Unix domain socket */
  strcpy(address.sun_path, SOCKET_ADDR);

  addrLength = sizeof(address.sun_family) + strlen(address.sun_path) + 1;

  assert((connect(sock, (struct sockaddr*)&address, addrLength)) == 0);
  return sock;
}


int main(int argc, char** argv) {
  setvbuf(stdin, NULL, _IOLBF, 0);
  setvbuf(stdout, NULL, _IOLBF, 0);

  printf("connecting to the bank, please wait.\n");
  int sock = connect_to_server();

  char server_response[BUFSIZE];
  if (read(sock, server_response, BUFSIZE - 1) > 0) {
    printf("%s", server_response);  // prints "ready" received from server
  }

  int quit = 0;
  char* buf = malloc(BUFSIZE);
  if (buf == NULL) return -1;
  while (!quit) {
    if (fgets(buf, BUFSIZE, stdin) == NULL) break;
    switch (buf[0]) {
    case 'q':
      quit = 1;
      break;
    default:
      write(sock, buf, strlen(buf));
      int n = read(sock, buf, BUFSIZE - 1);
      if (n > 0) {
        buf[n] = '\0';
        printf("%s", buf);
      }
      else if (n < 0) {
        perror("Read error");
        quit = 1;
      }

      break;
    }
  }
  free(buf);
  close(sock);
  return 0;
}

