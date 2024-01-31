#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "queue.h"
#include "logger.h"


#define THREAD_POOL_SIZE 10 // N service desks
#define SOCKET_ADDR "./socket"
#define DATABASE_FILE "accounts.csv"
#define LOG_FILE "server.log"
#define MAX_RESPONSE_LENGTH 50
#define MAX_REQUEST_LENGTH 50
#define MAX_DATABASE_FILE_LINE_LENGTH 100


volatile sig_atomic_t sigint_or_sigterm_received = 0;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread_pool[THREAD_POOL_SIZE];
FILE* log_file;


void initialize_database_file(void);
void get_balance(char* request, char* response);
void deposit(char* request, char* response);
void withdraw(char* request, char* response);
void transfer(char* request, char* response);
FILE* open_database_file(const char* mode);
void close_database_file(FILE* file);
void* thread_function(void* args);
void* handle_connection(void* p_client_socket);
void sigint_and_sigterm_handler(int sig);


int main(void) {
  log_file = fopen(LOG_FILE, "a");  // Open the log file in append mode
  if (log_file == NULL) {
    perror("Failed to open log file");
    return 1;
  }

  log_with_timestamp(log_file, "Server started");

  // signal handlers for graceful exiting
  struct sigaction sa;
  sa.sa_handler = sigint_and_sigterm_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  initialize_database_file();

  struct sockaddr_un address;
  int sock, client_socket;
  socklen_t addrLength;

  sock = socket(PF_UNIX, SOCK_STREAM, 0);
  unlink(SOCKET_ADDR);

  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, SOCKET_ADDR);
  addrLength = sizeof(address.sun_family) + strlen(address.sun_path) + 1;

  bind(sock, (struct sockaddr*)&address, addrLength);
  listen(sock, 5);

  queue_t* queues = malloc(THREAD_POOL_SIZE * sizeof(queue_t)); // dynamic allocation for queues

  if (queues == NULL) {
    perror("failed to allocate memory for queues");
    return 1;
  }

  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    // create threads
    pthread_create(&thread_pool[i], NULL, thread_function, &queues[i]);

    // create queues
    queues[i].head = NULL;
    queues[i].tail = NULL;
    queues[i].size = 0;
  }

  while (1) {
    printf("waiting for connections...\n");
    client_socket = accept(sock, (struct sockaddr*)&address, &addrLength);

    if (sigint_or_sigterm_received) {
      // printf("\nshutting down\n");
      break;
    }

    if (client_socket == -1) {
      if (errno == EINTR) {
        printf("accept() interrupted by signal\n");
        continue;
      }
      else {
        perror("error with socket");
      }
    }
    else {
      int* pclient = malloc(sizeof(int));
      *pclient = client_socket;

      // place the connection in the queue with the shortest length
      int shortest_queue_index = 0;
      for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (queues[i].size < queues[shortest_queue_index].size) {
          shortest_queue_index = i;
        }
      }
      enqueue(&queues[shortest_queue_index], pclient);
      printf("connected and placed in to queue #%d\n", shortest_queue_index);
      log_with_timestamp(log_file, "New connection placed in queue");
    }
  }

  printf("\nreceived shutdown signal. waiting for all clients to disconnect before exiting...\n");

  // wait for threads to finish
  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_join(thread_pool[i], NULL);
  }

  printf("shutting down...\nall threads have been joined\nexiting\n");
  log_with_timestamp(log_file, "Program exiting");

  // cleanup
  free(queues);
  fclose(log_file);

  return 0;
}

void* thread_function(void* queue) {
  while (!sigint_or_sigterm_received) {
    int* p_client = dequeue(queue);
    if (p_client != NULL) {
      handle_connection(p_client);
    }
  }
  return NULL;
}

void* handle_connection(void* p_client_socket) {
  int client_socket = *((int*)p_client_socket); // cast void pointer to int to satisfy compiler
  free(p_client_socket);

  // send "ready" to client
  const char* ready_message = "ready\n";
  write(client_socket, ready_message, strlen(ready_message));

  char request[MAX_REQUEST_LENGTH];
  char response[MAX_RESPONSE_LENGTH];
  int read_size;

  while ((read_size = read(client_socket, request, sizeof(request) - 1)) > 0 && !sigint_or_sigterm_received) {
    request[read_size] = '\0';

    switch (request[0]) {
    case 'l': {
      get_balance(request, response);
      break;
    }
    case 'w': {
      withdraw(request, response);
      break;
    }
    case 't':
      transfer(request, response);
      break;
    case 'd': {
      deposit(request, response);
      break;
    }
    default:
      strcpy(response, "fail: incorrect command\n");
      break;
    }
    write(client_socket, response, strlen(response));

    memset(request, 0, sizeof(request));
    memset(response, 0, sizeof(response));
  }

  if (read_size == 0) {
    printf("client disconnected\n");
    fflush(stdout);
  }
  else if (read_size == -1) {
    perror("recv failed");
  }

  close(client_socket);

  return NULL;
}

void initialize_database_file(void) {
  FILE* file = fopen(DATABASE_FILE, "r");
  if (file == NULL) {
    file = fopen(DATABASE_FILE, "w");
    if (file == NULL) {
      exit(-1);
    }
  }
  fclose(file);
}

FILE* open_database_file(const char* mode) {
  pthread_mutex_lock(&file_mutex);
  FILE* file = fopen(DATABASE_FILE, mode);
  if (file == NULL) {
    pthread_mutex_unlock(&file_mutex);
  }
  return file;
}

void close_database_file(FILE* file) {
  fclose(file);
  pthread_mutex_unlock(&file_mutex);
}

void get_balance(char* request, char* response) {
  int account_number = -1;
  if (sscanf(request, "l %d", &account_number) != 1) {
    strcpy(response, "fail: command incorrect\n");
    return;
  }
  FILE* file = open_database_file("r");
  if (file == NULL) {
    sprintf(response, "fail: error with database file\n");
    return;
  }
  char line[MAX_DATABASE_FILE_LINE_LENGTH];
  int balance = 0;
  while (fgets(line, sizeof(line), file)) {
    int current_account;
    int current_balance;
    if (sscanf(line, "%d,%d", &current_account, &current_balance) == 2) {
      if (current_account == account_number) {
        balance = current_balance;
        break;
      }
    }
  }
  close_database_file(file);
  sprintf(response, "ok: balance: %d\n", balance);
}

void deposit(char* request, char* response) {
  int account_number = -1, deposit_amount = 0;
  if (sscanf(request, "d %d %d", &account_number, &deposit_amount) != 2) {
    strcpy(response, "fail: command incorrect\n");
    return;
  }
  if (deposit_amount < 0) {
    strcpy(response, "fail: amount cannot be negative\n");
    return;
  }
  FILE* file = open_database_file("r+");
  if (file == NULL) {
    sprintf(response, "fail: error with database file\n");
    return;
  }
  char line[MAX_DATABASE_FILE_LINE_LENGTH];
  long int pos = 0;
  int found = 0;
  while (fgets(line, sizeof(line), file)) {
    int current_account, current_balance;
    if (sscanf(line, "%d,%d", &current_account, &current_balance) == 2) {
      if (current_account == account_number) {
        current_balance += deposit_amount;
        found = 1;
        fseek(file, pos, SEEK_SET);
        fprintf(file, "%30d,%30d\n", account_number, current_balance);
        sprintf(response, "ok: deposited %d\n", deposit_amount);
        break;
      }
    }
    pos = ftell(file);
  }

  if (!found) {
    // account not found, create it with the deposit amount
    fseek(file, 0, SEEK_END); // go to the end of the file
    fprintf(file, "%30d,%30d\n", account_number, deposit_amount);
    sprintf(response, "ok: deposited %d\n", deposit_amount);
  }
  close_database_file(file);
  sprintf(response, "ok: deposited: %d\n", deposit_amount);
}

void withdraw(char* request, char* response) {
  int account_number = -1, withdraw_amount = 0;
  if (sscanf(request, "w %d %d", &account_number, &withdraw_amount) != 2) {
    strcpy(response, "fail: command incorrect\n");
    return;
  }

  if (withdraw_amount < 0) {
    strcpy(response, "fail: amount cannot be negative\n");
    return;
  }

  FILE* file = open_database_file("r+");
  if (file == NULL) {
    sprintf(response, "fail: error with database file\n");
    return;
  }
  char line[MAX_DATABASE_FILE_LINE_LENGTH];
  long int pos = 0;
  int found = 0;
  while (fgets(line, sizeof(line), file)) {
    int current_account, current_balance;
    if (sscanf(line, "%d,%d", &current_account, &current_balance) == 2) {
      if (current_account == account_number) {
        found = 1;
        if (current_balance >= withdraw_amount) {
          current_balance -= withdraw_amount;
          fseek(file, pos, SEEK_SET);
          fprintf(file, "%30d,%30d\n", account_number, current_balance);
          sprintf(response, "ok: withdrew %d\n", withdraw_amount);
        }
        else {
          sprintf(response, "fail: insufficient balance\n");
        }
        break;
      }
    }
    pos = ftell(file);
  }

  if (!found) {
    sprintf(response, "fail: account not found\n");
  }
  close_database_file(file);
}

void transfer(char* request, char* response) {
  int from_account = -1, to_account = -1, amount = 0;
  if (sscanf(request, "t %d %d %d", &from_account, &to_account, &amount) != 3) {
    strcpy(response, "fail: command incorrect\n");
    return;
  }

  if (from_account == to_account) {
    strcpy(response, "fail: transfer only between different accounts\n");
    return;
  }

  if (amount < 0) {
    strcpy(response, "fail: amount cannot be negative\n");
    return;
  }

  FILE* file = open_database_file("r+");
  if (file == NULL) {
    strcpy(response, "fail: error with database file\n");
    return;
  }

  char line[MAX_DATABASE_FILE_LINE_LENGTH];
  long int from_pos = -1, to_pos = -1;
  int from_found = 0, to_found = 0;
  int from_balance = 0, to_balance = 0;

  while (fgets(line, sizeof(line), file)) {
    int current_account, current_balance;
    long int current_pos = ftell(file) - strlen(line);
    if (sscanf(line, "%d,%d", &current_account, &current_balance) == 2) {
      if (current_account == from_account) {
        from_pos = current_pos;
        from_balance = current_balance;
        from_found = 1;
      }
      if (current_account == to_account) {
        to_pos = current_pos;
        to_balance = current_balance;
        to_found = 1;
      }
    }
  }

  if (!from_found || !to_found) {
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    strcpy(response, from_found ? "fail: to account not found\n" : "fail: from account not found\n");
    return;
  }

  if (from_balance < amount) {
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    strcpy(response, "fail: insufficient balance\n");
    return;
  }

  fseek(file, from_pos, SEEK_SET);
  fprintf(file, "%30d,%30d\n", from_account, from_balance - amount);
  fseek(file, to_pos, SEEK_SET);
  fprintf(file, "%30d,%30d\n", to_account, to_balance + amount);

  close_database_file(file);
  sprintf(response, "ok: transferred %d from %d to %d\n", amount, from_account, to_account);
}

void sigint_and_sigterm_handler(int sig) {
  sigint_or_sigterm_received = 1;
}
