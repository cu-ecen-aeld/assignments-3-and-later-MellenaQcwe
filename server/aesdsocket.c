#include <aesdsocket.h>

slist_data_t *thd = NULL;
SLIST_HEAD(slisthead, slist_data_s) head;

void signal_handle(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) {
        syslog(LOG_NOTICE, "Caught signal, exiting.");

        remove(persistent_file);

        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);

        pthread_mutex_lock(&mutex);
        thd_exit_requested = 1;
        pthread_mutex_unlock(&mutex);

        while (!SLIST_EMPTY(&head)) {
            thd = SLIST_FIRST(&head);
            //printf("SIGNAL EXIT: Thread id  %lu\n", thd->data.id);
            if (thd != NULL) {
                pthread_join(thd->data.id, (void **)&thd->data.retval);
                if (thd->data.retval != NULL) {
                    //printf("SIGNAL EXIT: Thread retval %d\n", *(thd->data.retval));
                    free(thd->data.retval);
                }
                SLIST_REMOVE_HEAD(&head, entries);
                free(thd);
            }
        }
    }

    exit(0);
}

void cleanup() {
    freeaddrinfo(result);
}


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

    // Reuse socket port if not close
    int opt_enable = 1;
    if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof (opt_enable))) != 0) { 
        printf("Failed to set socket option SO_REUSEADDR.\n"); 
        exit(-1); 
    }

    // Disable Nagle's algorithm's delay
    if ((setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &opt_enable, sizeof (opt_enable))) != 0) { 
        printf("Failed to set socket option TCP_NODELAY.\n"); 
        exit(-1); 
    }

    // Increase sock buffer size to prevent recv routine to block waiting for more incoming data
    char desirable_buff_size_str[50] = {};
    int desirable_buff_size = MAX_PACKAGE_LEN*50; // cat /proc/sys/net/core/rmem_max retunrs 212992 and 4096*50 is 204800
    if ((read_from_file("/proc/sys/net/core/rmem_max", desirable_buff_size_str)) > 0) {
        desirable_buff_size = atoi(desirable_buff_size_str);
    }
    printf("Socket desirable data size is %d.\n", desirable_buff_size); 
    if ((setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &desirable_buff_size, sizeof (desirable_buff_size))) != 0) { 
        printf("Failed to set socket option SO_RCVBUF.\n"); 
        exit(-1); 
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
    
    // Init threads list
    SLIST_INIT(&head);

    // Start timestamp thread
    thd = malloc(sizeof(slist_data_t));
    thd->data.connfd = -1; // No socket
    thd->data.ip = NULL;
    thd->data.completed = false;
    pthread_create(&(thd->data.id), NULL, log_current_time, (void *)&thd->data);
    SLIST_INSERT_HEAD(&head, thd, entries);
    printf("Created thread %lu for time logging.\n", thd->data.id); 


    // Accept socket connections forever
    struct sockaddr_in client = {}; 
    socklen_t len = -1; 
    int connfd = -1;
    len = sizeof(client); 
    for (;;) {
        printf("Wait for connection.\n");
        connfd = accept(sockfd, (struct sockaddr *)&client, &len); 
        if (connfd < 0) { 
            printf("Failed to accept a connection.\n"); 
        } else {
            printf("Accepted connection from %s (fd=%d).\n", inet_ntoa(client.sin_addr), connfd); 
            syslog(LOG_NOTICE, "Accepted connection from %s (fd=%d).\n", inet_ntoa(client.sin_addr), connfd); 

            thd = malloc(sizeof(slist_data_t));
            thd->data.connfd = connfd;
            thd->data.ip = inet_ntoa(client.sin_addr);
            thd->data.completed = false;
            pthread_create(&(thd->data.id), NULL, msg_exchange, (void *)&thd->data);
            SLIST_INSERT_HEAD(&head, thd, entries);
            printf("Created thread %lu for socket connfd %d.\n", thd->data.id, connfd); 
            
            SLIST_FOREACH(thd, &head, entries) {
                if (thd != NULL) {
                    //printf("Thread %lu completed flag is %d\n", thd->data.id, thd->data.completed);
                    if (thd->data.completed) {
                        pthread_join(thd->data.id, (void **)&thd->data.retval);
                        if (thd->data.retval != NULL) {
                            printf("Thread %lu complete with retval %d\n", thd->data.id, *(thd->data.retval));
                            free(thd->data.retval);
                        }
                        SLIST_REMOVE(&head, thd, slist_data_s, entries);
                        free(thd);
                    }
                }
            }
        }
    }

    return 0;
}

static void print_usage(const char* command_name) {
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
    //printf("Wrote '%d' bytes to file '%s'.\n", sz, user_file);
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

    // Read file content
    fseek(fptr, 0, SEEK_END); // Move file position to the end of the file
    file_sz = ftell(fptr); // Get the current file position
    fseek(fptr, 0, SEEK_SET); // Reset file position to start of file
    sz = fread(read_buff, 1, file_sz, fptr);
    if (sz <= 0) {
        printf("Failed read from file %s.\n", user_file);
        return -1;
    }
    if ((long int)sz != file_sz) {
        printf("Warning: read %ld bytes != %ld file size.\n", sz, file_sz);
    }
    //printf("Read '%ld' bytes from file '%s'.\n", sz, user_file);

    // Close the file
    if (fclose(fptr) < 0) {
        printf("Failed to close %s.\n", user_file);
        return -1;
    }

    return sz;
}

static void* msg_exchange(void *_args) { 
    /**
     * Function designed for msg exchange between client and server. 
     * @param arg1 The socket connection to client with client_ip
     * @param arg2 The ip of the socket client
     * @return Void
     */
    
    struct thread_data *args = (struct thread_data *)_args;
    int connfd = args->connfd;
    char* clientip = args->ip;
    int *retval = (int *)malloc(sizeof (int));
    *retval = 0;

    char *buff = (char*)malloc(MAX_PACKAGE_LEN); // Allocate buffer for new thread
    ssize_t read_buff_total_len = 0;
    ssize_t send_buff_current_len = 0;
    ssize_t send_buff_total_len = 0;

    while (!thd_exit_requested) {
        // Read the message from client non blocking and copy it in buffer 
        memset(buff, '\0', MAX_PACKAGE_LEN);
        read_buff_total_len = recv(connfd, buff, MAX_PACKAGE_LEN, MSG_DONTWAIT /*none blocking io*/); 
        if (read_buff_total_len == -1 && errno == EAGAIN) {
            //printf("Waiting for incoming data from client fd %d.\n", connfd); 
            continue;
        } else if (read_buff_total_len == 0) {
            printf("Closed connection from %s (fd=%d).\n", clientip, connfd); 
            syslog(LOG_NOTICE, "Closed connection from %s (fd=%d).\n", clientip, connfd); 
            break; // goto thread_exit
        } else if (read_buff_total_len > MAX_PACKAGE_LEN) {
            printf("Failed with packet length exeeding %d bytes: Discarded ((client fd %d)).\n", (int)MAX_PACKAGE_LEN, connfd); 
            *retval = 1;
            break; // goto thread_exit
        } else {
            if (read_buff_total_len > 0) {
                // Check package termination (newline)
                if (buff[read_buff_total_len-1] == '\n') {
                    printf("Received package from client fd %d: %s", connfd, buff); 

                    // Write package to persistance file
                    pthread_mutex_lock(&mutex);
                    if (write_to_file(persistent_file, buff) == -1) {
                        printf("Failed to log message to persistant file.\n");
                        *retval = 1;
                        break; // goto thread_exit
                    } else {
                        // Send all packages to the client
                        memset(buff, '\0', MAX_PACKAGE_LEN);
                        read_buff_total_len = read_from_file(persistent_file, buff);

                        if (read_buff_total_len != -1) {
                            while(send_buff_total_len < read_buff_total_len) { 
                                //printf("Sending all packages to client fd %d.\n", connfd);
                                send_buff_current_len = send(connfd, buff, read_buff_total_len, MSG_DONTWAIT);
                                if (send_buff_current_len == -1 && errno == EAGAIN) {
                                    continue;
                                }
                                send_buff_total_len += send_buff_current_len; 
                            }
                            //printf("%ld bytes sent.\n", send_buff_total_len); 
                        } else {
                            printf("Failed to read all packages from persistant file.\n");
                            *retval = 1;
                            break; // goto thread_exit
                        }
                    }
                    pthread_mutex_unlock(&mutex);
                } else {
                    printf("Failed to parse package: Missing package termination \\n from client fd %d.\n", connfd); 
                }
            }
        }
    }

    args->completed = true;

    free(buff);

    shutdown(connfd, SHUT_RDWR);
    close(connfd);

    pthread_mutex_lock(&mutex);
    if (thd_exit_requested) *retval = 2; // Parent caught signal exit
    pthread_mutex_unlock(&mutex);

    printf("Thread with socket fd=%d exit rc=%d.\n", connfd, *retval); 
    pthread_exit((void*)retval);
} 

void * log_current_time(void *_args) {
    /**
     * Function designed to log the current system time to persistent file 
     * @param _args Paramter list
     * @return Void pointer holding the return value
     */

    struct thread_data *args = (struct thread_data *)_args;
    struct tm *tmp;
    char timestamp[50];
    time_t rawtime;
    struct timespec tspec = { .tv_sec=10, .tv_nsec=0 }; // Sleep for 10 secs
    int *retval = (int *)malloc(sizeof (int));
    *retval = 0;

    while (!thd_exit_requested) {
        if ((clock_nanosleep(CLOCK_MONOTONIC, 0, &tspec, NULL)) == 0) {
            rawtime = time(NULL);
            tmp = localtime(&rawtime);
            if (tmp == NULL) {
                printf("Failed to get local time.\n"); 
                * retval = 1;
                break; // goto thread_exit
            }
            if ((strftime(timestamp, sizeof (timestamp), "timestamp:%Y-%m-%d %H:%M:%S\n", tmp) != 0)) {
                //printf("Logging timestamp: %s", timestamp); 
                pthread_mutex_lock(&mutex);
                if (write_to_file(persistent_file, timestamp) == -1) {
                    printf("Failed to log timestamp into persistant file.\n");
                    *retval = 1;
                    break; // goto thread_exit
                } 
                pthread_mutex_unlock(&mutex);
            } else {
                // 0 Bbytes written
                printf("Failed to get timestamp into buffer.\n"); 
                *retval = 1;
                break; // goto thread_exit
            }
        }
    }

    args->completed = true;

    pthread_mutex_lock(&mutex);
    if (thd_exit_requested) *retval = 2; // Parent caught signal exit
    pthread_mutex_unlock(&mutex);
    //printf("Thread for time logging exit rc=%d.\n", *retval); 

    pthread_exit((void*)retval);
}