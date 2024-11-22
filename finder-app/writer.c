#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <syslog.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int help_flag;

static void print_usage (const char*);
static int write_to_file (const char*, const char*);

int main(int argc, char* argv[]) {
    if (argc < 2) print_usage (argv[0]);

    openlog ("CourseraAssignment2::Writer", LOG_PID | LOG_LOCAL0, LOG_USER);
    syslog (LOG_NOTICE, "Program started by User %d", getuid ());

    static struct option long_options[] =
    {
        /* These options set a flag. */
        {"help",        no_argument,            &help_flag,       1},
        /* These options don’t set a flag.*/
        {"test",        no_argument,            0,              't'},
        {"writefile",   required_argument,      0,              'f'},
        {"writestr",    required_argument,      0,              's'},
        {0, 0, 0, 0}
    };

    int option;
    int option_index = 0;
    char *file_path = NULL;
    char *str = NULL;
    while ((option = getopt_long (argc, argv, "htf:s:", long_options, &option_index)) != -1){
        if (help_flag) print_usage (argv[0]);
        
        switch (option)
        {
        case 't':
            printf ("command '%s' works!\n", long_options[option_index].name);
            break;
        case 'f':
            file_path = optarg;
            break;
        case 's':
            str = optarg;
            break;
        }
    }

    /* Print any remaining command line arguments (not options). */
    if (optind < argc) {
        printf ("Unrecognized option: ");
        while (optind < argc) printf ("%s ", argv[optind++]);
        putchar ('\n');
        exit(1);
    }

    if (file_path && str) { 
        int rc = 1;
        if ((rc = write_to_file (file_path, str) != 0)) exit(rc); 
    }

    syslog (LOG_NOTICE, "Program end by User %d", getuid ());
    closelog();
    return 0;
}

static void print_usage (const char* command_name) {
    printf ("Usage: %s <writefile> <writestr>\n", command_name);
    printf ( "Parameters:\n");
    printf ( "<writefile> : The Full path of the file to be created or overriden<\n.");
    printf ( "<writestr> : The string to be written in <writefile>.\n");
    exit(0);
}

static int write_to_file (const char* file_path, const char* str) {
    /*Assuming the directory is created by the caller*/
    int fptr = open (file_path, O_WRONLY | O_CREAT);
    if (fptr == -1) {
        printf ("%s does not exist. Please create the file before running this program.", file_path);
        syslog (LOG_ERR, "%s does not exist. Please create the file before running this program.", file_path);
        return 1;
    }
    int sz = write (fptr, str, strlen(str));
    printf("Written '%s' to file '%s' (%d bytes)\n", str, file_path, sz);
    syslog (LOG_DEBUG, "Written '%s' to file '%s' (%d bytes)\n", str, file_path, sz);
    if (close (fptr) < 0) return 1;

    return 0;
}