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

#define MAX_PACKAGE_LEN 1024*100  // 100 Kbytes
#define PORT "9000"

static struct addrinfo *result = NULL;
static int sockfd = -1; fd_set active_fds = {};
static char persistent_file[] = "/var/tmp/aesdsocketdata";
static int daemon_flag = 0;
static int help_flag = 0;


void signal_handle(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) {
        printf("Caught signal, exiting.\n"); 
        syslog(LOG_NOTICE, "Caught signal, exiting.");
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd); 
        FD_CLR(sockfd, &active_fds);
        remove(persistent_file);
        exit(0);
    }
}

void cleanup() {
    freeaddrinfo(result);
}

static void msg_exchange(int, char *);
static int write_to_file(const char*, const char*);
static ssize_t read_from_file(const char*, char*);
static void print_usage (const char*);
static void parse_cmdline_args(int, char *[]);

int main(int argc, char *argv[]) { 
    parse_cmdline_args(argc, argv);

    // register all callback funcs
    atexit(cleanup);
    signal(SIGINT, signal_handle);
    signal(SIGTERM, signal_handle);

    // Init syslog
    openlog("CourseraAssignment5::Server", LOG_PID | LOG_LOCAL0, LOG_USER);
    syslog(LOG_NOTICE, "Server started by User %d", getuid ());

    // Get suiltable sockaddr for bind() and accept using getaddrinfo()
    struct addrinfo hints = {};
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, PORT, &hints, &result) != 0) {
        printf("Failed to get addrinfo.\n"); 
        exit(-1); 
    }

    // socket create and verification 
    sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol); 
    if (sockfd == -1) { 
        printf("Failed to open stream socket.\n"); 
        exit(-1); 
    } else {
        printf("Socket successfully created.\n"); 
    }
  
    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *)result->ai_addr, result->ai_addrlen)) != 0) { 
        printf("Failed to bind socket.\n"); 
        exit(-1); 
    } else {
        printf("Socket successfully binded.\n"); 
    }

    // Fork a new process and exit this parent process
    if (daemon_flag) {
        printf("Running in background.\n"); 
        if ((daemon(0, 0)) == -1) {
            printf("Failed to enter daemon mode.\n"); 
            exit(-1);
        }
    }
  
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        printf("Failed to listen for entring connection.\n"); 
        exit(-1); 
    } else {
        printf("Server listening.\n"); 
    }
  
    struct sockaddr_in clients[FD_SETSIZE] = {}; 
    struct sockaddr_in client = {}; 
    FD_ZERO(&active_fds);
    FD_SET(sockfd, &active_fds);
    fd_set read_fds = {};
    socklen_t len = -1; 
    len = sizeof(client); 
    int connfd = -1;
    for (;;) {
        // Block until input arrive on one or more active sockets
        read_fds = active_fds;
        //printf("Waiting for new active socket.\n"); 
        if ((select(FD_SETSIZE, &read_fds, NULL, NULL, NULL)) < 0) {
            printf("No active socket. Continue.\n"); 
            continue;
        }
        for (int fd = 0; fd < FD_SETSIZE; ++fd) {
            if (FD_ISSET(fd, &read_fds)) {
                //printf("Found active socket fd %d.\n", fd); 
                if (fd == sockfd) {
                    // Connection request on server socket (Interrupt caught for sockfd)
                    connfd = accept(sockfd, (struct sockaddr *)&client, &len); 
                    if (connfd < 0) { 
                        printf("Failed to accept a connection.\n"); 
                    } else {
                        printf("Accepted connection from %s (fd=%d).\n", inet_ntoa(client.sin_addr), connfd); 
                        syslog(LOG_NOTICE, "Accepted connection from %s (fd=%d).\n", inet_ntoa(client.sin_addr), connfd); 
                        FD_SET(connfd, &active_fds);
                        clients[fd] = client;
                    }
                } else {
                    // Data arrived on already connected socket (Interrupt caught for fd)
                    msg_exchange(fd, inet_ntoa(clients[fd].sin_addr)); 
                }
            }
        }
    }
    
    return 0;
}

static void print_usage (const char* command_name) {
    printf ("Usage: %s <option>\n", command_name);
    printf ( "Options:\n");
    printf ( "-d : Run in background.\n");
    printf ( "--help : Print this help.\n");
    exit(0);
}

static void parse_cmdline_args(int argc, char *argv[]) {

    static struct option long_options[] =
    {
        // These options set a flag
        {"help",    no_argument,    &help_flag,  1},
        {"daemon",  no_argument,    &daemon_flag, 1},
        {0, 0, 0, 0}
    };

    int option = -1;
    int option_index = 0;
    while ((option = getopt_long (argc, argv, "hd", long_options, &option_index)) != -1){
        switch (option)
        {
        case 'h':
            print_usage(argv[0]);
            break;
        case 'd':
            daemon_flag = 1;
            break;
        default:
            break;
        }
    }

    if (help_flag) {
        print_usage(argv[0]);
    }

    // Print any remaining command line arguments (not options)
    if (optind < argc) {
        printf ("Unrecognized option: ");
        while (optind < argc) printf ("%s ", argv[optind++]);
        putchar ('\n');
        exit(0);
    }
}

static int write_to_file(const char* user_file, const char* str) {

    /**
     * Log messge to file
     * @param user_file File to write to 
     * @param str The message to log
     * @return Return the number of bytes written, or -1 if an error occure
     */

    int fptr = -1;
    int sz = -1;
    
    // Append to already created file
    fptr = open(user_file, O_WRONLY | O_APPEND | O_CREAT, 0600);

    if (fptr == -1) {
        printf("Failed to open %s.\n", user_file);
        return -1;
    }

    // Write the buffer to the file
    sz = write(fptr, str, strlen(str));
    printf("Wrote '%d' bytes to file '%s'.\n", sz, user_file);
    if (sz == -1) {
        printf("Failed to write to file.\n");
        return -1;
    }

    // Close the file
    if (close(fptr) < 0) {
        printf("Failed to close %s.\n", user_file);
        return -1;
    }

    return sz;
}

static ssize_t read_from_file(const char* user_file, char* read_buff) {

    /**
     * Read file content
     * @param user_file Write to read from
     * @param str The memory to store the file content
     * @return  Return the number of bytes read, or -1 if an error occure
     */

    FILE *fptr = NULL;
    size_t sz = -1;
    long int file_sz = -1;
    
    // Open read only
    fptr = fopen(user_file, "r");
    if (fptr == NULL) {
        printf("Failed to open %s.\n", user_file);
        return -1;
    }

    // Check file permission
    struct stat st = {};
    stat(user_file, &st);
    printf("File permissions:  %04o.\n", st.st_mode & 0777);

    // Read file content
    fseek(fptr, 0, SEEK_END); // Move file position to the end of the file
    file_sz = ftell(fptr); // Get the current file position
    fseek(fptr, 0, SEEK_SET); // Reset file position to start of file
    sz = fread(read_buff, 1, file_sz, fptr);
    if ((long int)sz != file_sz) {
        printf("Warning: read %ld bytes != %ld file size.\n", sz, file_sz);
    }
    printf("Read '%ld' bytes from file '%s'.\n", sz, user_file);

    // Close the file
    if (fclose(fptr) < 0) {
        printf("Failed to close %s.\n", user_file);
        return -1;
    }

    return sz;
}

static void msg_exchange(int connfd, char* clientip) { 
    /**
     * Function designed for msg exchange between client and server. 
     * @param connfd The socket connection to client with client_ip
     * @param clientip The ip of the socket client
     * @return Void
     */
    
    char buff[MAX_PACKAGE_LEN+1] = {}; // One byte padding for '\0' termination
    ssize_t buff_len = 0;
    char buff_offset = 0;
    bzero(buff, MAX_PACKAGE_LEN+1); 
    char *read_buff = (char*)malloc(MAX_PACKAGE_LEN);
    ssize_t read_buff_total = 0;
    ssize_t send_buff_len = 0;
    ssize_t send_buff_total = 0;

    // Read the message from client non blocking and copy it in buffer 
    buff_len = recv(connfd, (buff + buff_offset), sizeof(buff), MSG_DONTWAIT); 
    if (buff_len == -1 && errno == EAGAIN) {
        printf("Waiting for incoming data from client fd %d.\n", connfd); 
    } else if (buff_len == 0) {
        FD_CLR(connfd, &active_fds);
        printf("Closed connection from %s (fd=%d).\n", clientip, connfd); 
        syslog(LOG_NOTICE, "Closed connection from %s (fd=%d).\n", clientip, connfd); 
    } else if (buff_len > MAX_PACKAGE_LEN) {
        printf("Packet from client fd %d exeeds length of %d bytes: Discarded.\n", connfd, (int)MAX_PACKAGE_LEN); 
    } else {
        if (buff_len > 0) {
            // Check string termination ('\0')
            if (strlen(buff) != (size_t)(buff_len-1)) {
                buff[buff_len] = '\0'; // padding string termination
            }

            // Check package termination (newline)
            if (buff[strlen(buff)-1] == '\n') {
                printf("Received package from client fd %d: %s", connfd, buff); 

                // Write package to persistance file
                if (write_to_file(persistent_file, buff) == -1) {
                    printf("Failed to log message to persistant file.\n");
                } else {
                    // Send all packages to the client
                    read_buff_total = read_from_file(persistent_file, read_buff);

                    if (read_buff_total != -1) {
                        while(send_buff_total < read_buff_total) { 
                            printf("Sending all packages to client fd %d.\n", connfd);
                            //printf("Packages:\n%s", read_buff);  
                            send_buff_len = send(connfd,read_buff, read_buff_total, MSG_DONTWAIT);
                            if (buff_len == -1 && errno == EAGAIN) {
                                continue;
                            }
                            send_buff_total += send_buff_len; 
                        }
                        printf("%ld bytes sent.\n", send_buff_total); 
                    } else {
                        printf("Failed to read all packages from persistant file.\n");
                    }
                }
            } else {
                printf("Missing package termination (\n) from client fd %d.\n", connfd); 
            }
        }
    }

    free(read_buff);
} 