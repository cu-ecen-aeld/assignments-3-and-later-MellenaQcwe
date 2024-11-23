#include "systemcalls.h"
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int rc;
    if ((rc = system(cmd) != 0)) { perror(cmd); return false; }
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{ 
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    bool success = true;
    int pid;
    fflush(stdout);
    pid = fork();
    if (pid == 0/*Child*/) {
        execv (command[0], command); /*Replace Child process with shell command process*/
        //perror("execv() failed"); success = false; /*If this code is reached then execv() failed with rc=-1*/
        // Above code commented out beacuse it leads child to exit with status=0(success), which leads to unexpected behavior
    } else if (pid > 0/*Parent*/) {
        int status;
        if (waitpid(pid, &status, 0) == -1/*Wait for child to terminate*/) { perror("wait() failed"); success = false; }
        else { 
            if (WIFEXITED(status)) { 
                int rc = WEXITSTATUS(status);
                printf("Child exited, rc=%d\n", rc); 
                success = (rc == 0) ? true : false;
            } else if (WIFSIGNALED(status)) { printf("Child killed by signal %d\n", WTERMSIG(status)); 
            } else if (WIFSTOPPED(status)) { printf("Child stopped by signal %d\n", WSTOPSIG(status)); 
            } else if (WIFCONTINUED(status)) { printf("Child continued\n"); }
         }
    } else { perror ("fork() failed"); success = false; }

    va_end(args);

    return success;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int fd = open (outputfile, O_WRONLY | O_CREAT, 644);
    if (fd < 0) { perror ("open() failed"); return false; }

    bool success = true;
    int pid;
    fflush(stdout);
    pid = fork();
    if (pid == 0/*Child*/) {
        if (dup2(fd, 1/*stdout*/) < 0) { perror("dups() failed"); success = false; }
        else {
            execv (command[0], command); /*Replace Child process with shell command process*/
            //perror("execv() failed"); success = false; /*If this code is reached then execv() failed with rc=-1*/
            // Above code commented out beacuse it leads child to exit with status=0(success), which leads to unexpected behavior
        }
    } else if (pid > 0/*Parent*/) {
        int status;
        if (wait(&status) == -1/*Wait for child to terminate*/) { perror("wait() failed"); success = false; }
        else { 
            if (WIFEXITED(status)) { 
                int rc = WEXITSTATUS(status);
                printf("Child exited, rc=%d\n", rc); 
                success = rc == 0 ? true : false;
            } else if (WIFSIGNALED(status)) { printf("Child killed by signal %d\n", WTERMSIG(status)); 
            } else if (WIFSTOPPED(status)) { printf("Child stopped by signal %d\n", WSTOPSIG(status)); 
            } else if (WIFCONTINUED(status)) { printf("Child continued\n"); }
         }
    } else { perror ("fork() failed"); success = false; }

    close(fd);
    va_end(args);

    return success;
}
