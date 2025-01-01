#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

// Macro to handle errors
#define handle_error(msg)                                                 \
    fprintf(stderr, "Error at line %d in file %s: ", __LINE__, __FILE__); \
    perror(msg);                                                          \
    exit(EXIT_FAILURE);

// Macro to handle signals
#define handle_sigaction(handler, signal)       \
    struct sigaction sa;                        \
    sa.sa_flags = 0;                            \
    sa.sa_handler = handler;                    \
    if (sigaction(signal, &sa, NULL) == -1) {   \
        handle_error("sigaction"); }

// Function declarations
int process_arglist(int count, char **arglist);
int prepare(void);
int finalize(void);
void regular(char **arglist);
void background(int symbol_index, char **arglist);
void pipeline(int symbol_index, char **arglist);
void input_redirection(int symbol_index, char **arglist);
void output_redirection(int count, char **arglist);

/*
    @brief Initialze the environment before executing the commands
    @return 0 on success
*/
int prepare(void)
{
    // Ignore SIGINT for the shell
    handle_sigaction(SIG_IGN, SIGINT);

    return 0;
}

/*
    @brief Finalizing the environment after executing the commands
    @return 0 on success
*/
int finalize(void)
{
    return 0;
}

/*
    @brief Execute the commands writen in the shell
    @param count Number of words in the command line
    @param arglist Array of the parsed command line
    @return 1 on success
*/
int process_arglist(int count, char **arglist)
{
    int i, symbol_index, stat;

    int pid = fork();

    if (pid == -1) // fork() failure
    {
        handle_error("fork");
    }

    if (pid == 0) // Child
    {
        handle_sigaction(SIG_DFL, SIGINT);
    }

    for (i = 0; i < count; i++)
    {
        if (i == (count - 1) && strcmp(arglist[i], "&") == 0) // BACKGROUND
        {
            if (pid == 0) // Child
            {
                // Ignore SIGINT for the background child
                handle_sigaction(SIG_IGN, SIGINT);

                symbol_index = i;
                background(symbol_index, arglist);
            }

            else // Parent doesn't wait until child finishes -> the command runs in background
            {
                // Ignore SIGCHLD in the shell
                handle_sigaction(SIG_IGN, SIGCHLD);

                return 1;
            }
        }

        if (strcmp(arglist[i], "|") == 0) // PIPELINE
        {
            if (pid == 0) // Child
            {
                symbol_index = i;
                pipeline(symbol_index, arglist);
            }

            else // Parent
            {
                pid_t cpid = waitpid(pid, &stat, 0);
                if (cpid == -1 && errno != EINTR && errno != ECHILD)
                {
                    handle_error("waitpid");
                }
                return 1;
            }
        }

        if (i == (count - 2) && strcmp(arglist[i], "<") == 0) // INPUT
        {
            if (pid == 0) // Child
            {
                symbol_index = i;
                input_redirection(symbol_index, arglist);
            }

            else // Parent
            {
                pid_t cpid = waitpid(pid, &stat, 0);
                if (cpid == -1 && errno != EINTR && errno != ECHILD)
                {
                    handle_error("waitpid");
                }
                return 1;
            }
        }

        if (i == (count - 2) && strcmp(arglist[i], ">>") == 0) // OUTPUT
        {
            if (pid == 0) // Child
            {
                symbol_index = i;
                output_redirection(symbol_index, arglist);
            }

            else // Parent
            {
                pid_t cpid = waitpid(pid, &stat, 0);
                if (cpid == -1 && errno != EINTR && errno != ECHILD)
                {
                    handle_error("waitpid");
                }
                return 1;
            }
        }
    }

    // REGULAR
    if (pid == 0) // Child
    {
        regular(arglist);
    }

    else // Parent
    {
        pid_t cpid = waitpid(pid, &stat, 0);
        if (cpid == -1 && errno != EINTR && errno != ECHILD)
        {
            handle_error("waitpid");
        }
        return 1;
    }

    return 1;
}

/*
    @brief Execute regular command line
    @param arglist Array of the parsef command line
*/
void regular(char **arglist)
{
    execvp(arglist[0], arglist);
    // execvp failed
    if (errno == ENOENT) // The command doesnt exists
    {
        handle_error("bash");
    }
    handle_error("execvp"); // execvp failed
}

/*
    @brief Execute background command line
    @param symbol_index The index in arglist where the '&' is found
    @param arglist Array of the parsef command line
*/
void background(int symbol_index, char **arglist)
{
    arglist[symbol_index] = NULL;
    execvp(arglist[0], arglist);
    // execvp failed
    if (errno == ENOENT) // The command doesnt exists
    {
        handle_error("bash");
    }
    handle_error("execvp"); // execvp failed
}

/*
    @brief Execute pipeline command line
    @param symbol_index The index in arglist where the '|' is found
    @param arglist Array of the parsef command line
*/
void pipeline(int symbol_index, char **arglist)
{
    int pipefd[2];
    int cpid;

    arglist[symbol_index] = NULL;

    if (pipe(pipefd) == -1) // pipe() failure
    {
        handle_error("pipe");
    }

    cpid = fork();
    if (cpid == -1) // fork() failure
    {
        handle_error("fork");
    }

    if (cpid == 0) // Child
    {
        // The child do the second command in the pipeline
        close(pipefd[1]);   // Closing the write end of the pipe
        dup2(pipefd[0], 0); // Changing the stdin to the read end of the pipe
        close(pipefd[0]);   // Closing the read end of the pipe
        execvp(arglist[symbol_index + 1], (arglist + symbol_index + 1));
        // execvp failed
        if (errno == ENOENT) // The command doesnt exists
        {
            handle_error("bash");
        }
        handle_error("execvp"); // execvp failed
    }

    else // Parent
    {
        // The parent do the first command in the pipeline
        close(pipefd[0]);   // Closing read end of the pipe
        dup2(pipefd[1], 1); // Changing the stdout to the write end of the pipe
        close(pipefd[1]);   // Closing the write end of the pipe
        execvp(arglist[0], arglist);
        // execvp failed
        if (errno == ENOENT) // The command doesnt exists
        {
            handle_error("bash");
        }
        handle_error("execvp"); // execvp failed
        
    }
}

/*
    @brief Execute input redirection command line
    @param symbol_index The index in arglist where the '<' is found
    @param arglist Array of the parsef command line
*/
void input_redirection(int symbol_index, char **arglist)
{
    char *filename = arglist[symbol_index + 1];
    int file_desc = open(filename, O_RDONLY);

    arglist[symbol_index] = NULL;

    if (file_desc == -1) // open() failure
    {
        handle_error("open");
    }

    dup2(file_desc, 0); // Changing the stdin to the file
    close(file_desc);
    execvp(arglist[0], arglist);
    // execvp failed
    if (errno == ENOENT) // The command doesnt exists
    {
        handle_error("bash");
    }
    handle_error("execvp"); // execvp failed
}

/*
    @brief Execute outout redirection command line
    @param symbol_index The index in arglist where the first '>' is found
    @param arglist Array of the parsef command line
*/
void output_redirection(int symbol_index, char **arglist)
{
    char *filename = arglist[symbol_index + 1];
    int file_desc = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0644);

    arglist[symbol_index] = NULL;

    if (file_desc == -1) // open() failure
    {
        handle_error("open");
    }

    dup2(file_desc, 1); // Changing the stdout to the file
    close(file_desc);
    execvp(arglist[0], arglist);
    // execvp failed
    if (errno == ENOENT) // The command doesnt exists
    {
        handle_error("bash");
    }
    handle_error("execvp"); // execvp failed
}
