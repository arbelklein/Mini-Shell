# Mini-Shell

## Description
This repository contains a simple shell program designed to provide experience with process management, pipes, signals, and relevant system calls. The shell supports executing commands, executing commands in the background, single piping, input redirection, and appending redirected output.

## Usage
1. Compile the code using `gcc`:
    ```sh
    gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 shell.c myshell.c -o myshell
    ```
2. Run the compiled program:
    ```sh
    ./myshell
    ```

## Output
The shell program will execute user commands as specified. It supports:
- Executing commands and waiting for their completion.
- Executing commands in the background without waiting for their completion.
- Piping the output of one command to the input of another.
- Redirecting input from a file.
- Appending output to a file.
