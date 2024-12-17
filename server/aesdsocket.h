#ifndef AESD_SOCKET
#define AESD_SOCKET

#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <getopt.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <sys/queue.h>


#define MAX_PACKAGE_LEN 1024*4  // 4 Kbytes maximal length of byte received on socket
#define PORT "9000" // Socket port to bind to
#define MAX_THREADS 100 // Maximal allowed threads

static struct addrinfo *result = NULL; // Socket address info
static int sockfd = -1; // Server socket to listen for connection
static char persistent_file[] = "/var/tmp/aesdsocketdata"; // Persistent file
static int daemon_flag = 0; // Don't run in daemon mode (default)
static int help_flag = 0; // Enable commandline help output

// Thread data
static pthread_mutex_t mutex; // Lock and Unlock before and after write operation on persistent file
static volatile int thd_exit_requested = 0; // Send exit request to all created threads
struct thread_data {
    pthread_t id;
    int connfd; // cleint connection fd
    char *ip; // Client ip
    bool completed; // Flag to check the thread completed
    int *retval; // Hold retval
};
typedef struct slist_data_s slist_data_t;
struct slist_data_s {
    struct thread_data data;
    SLIST_ENTRY(slist_data_s) entries;
};

static void* msg_exchange(void *);
static int write_to_file(const char*, const char*);
static ssize_t read_from_file(const char*, char*);
static void print_usage (const char*);
static void parse_cmdline_args(int, char *[]);
static void* log_current_time(void *);

#endif // AESD_SOCKET