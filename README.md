# Thread Bank: a multithreaded application written in C that simulates a bank

I made this project to demonstrate and sharpen my skills in writing multithreaded C applications. The application contains two programs: a server and a client. The server acts as a bank which contains multiple service desks (threads). When a client connection is received it is either moved to one of the desks (threads) or placed in a queue to wait for a desk (thread) to become free. The clients can make different operations by commands at the desk which manipulate the state of the bank accounts and balances.

At the very end of this file there is a comprehensive description of the possible commands that the client can give.

## Structure

A brief descriptions of the files in this project.

### Program source code

- `Makefile`: Used to automate the compilation of the program.
- `server.c`: Define the server program that listens for connections from clients and processes requests from clients.
- `client.c`: Define the client program that can connect to the server.
- `queue.c` & `queue.h`: Define the queue datastructure and the functions enqueue and dequeue for the data structure.
- `logger.c` & `logger.h`: Implement logging functionality.

## Run the program

**IMPORTANT!** The socket path used to connect to the socket between the server and client is defined in both programs as a relative path `./socket`. Therefore to successfully connect from the client to the server you must run them from the same directory.

1. First compile by running `make`
2. Run the server program `./server`
3. Run the client program `./client`

To clean object files `make clean`

## Logs and data files

The program writes a log file `server.log`. The server writes the data for accounts and balances into a file `./accounts.csv`.

## Possible client commands

The server accepts different commands from the client. Here is a description of the commands:

| Command                                        | Description                                                                              | Example             |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------- | ------------------- |
| `l [account_no]`                               | Gives the balance of account `[account_no]  `                                            | `l 12345`           |
| `w [account_no] [amount]`                      | Withdraws `[amount]` euros from account `[account_no]`                                   | `w 12345 100`       |
| `t [from_account_no] [to_account_no] [amount]` | Transfers `[amount]` euros from account `[from_account_no]` to account `[to_account_no]` | `t 12345 67890 100` |
| `d [account_no] [amont]`                       | Deposits `[amount]` euros to account `[account_no]`                                      | `d 12345 100`       |
| `q`                                            | Quits and leaves the desk. This terminates the client connection and exits the client    | `q`                 |

When the client starts up, it prints out “ready” to notify that the client has reached the end of the queue and a desk is now serving the client.
