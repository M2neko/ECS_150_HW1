//
// sshell.c
//
// Created by on 2019/10/10.
// Copyright © 2019 Bingwei Wang & Qikai Huang. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "dirent.h"

#define MAXINPUT 512
#define MAXCWD 256
#define MAXCOMMAND 64
#define MAXFILENAME 100
#define MAXCMD 20
#define MAXPIPE 10
#define CMDNUM 13

char* CmdList[] = {
    "exit",
    "ls",
    "pwd",
    "cd",
    "date",
    "cat",
    "echo",
    "touch",
    "wc",
    "sleep",
    "mkdir",
    "rmdir",
    "grep"
};

// ErrorMessage
typedef enum {
    TOOMANYARGU,
    NODIR,
    NOCMD,
    MISSCMD,
    MISLOCATEI,
    NOI,
    CANTOPENI,
    MISLOCATEO,
    NOO,
    CANTOPENO,
    MISLOCATEB,
    ACTIVE
} Error;

// Differentiate < > | &
typedef enum {
    NORMAL,
    INPUT,
    OUTPUT,
    AMPERSAND
} Signal;

// The main struct includes the command line, the input or output redirection
// filename, the number of arguments in the command line, and the signal
typedef struct command {
    char *process[MAXCOMMAND];
    char filename[MAXFILENAME];
    int argc;
    Signal Message;
} Command;

// Print ErrorMessage
void print_error(Error eMessage) {
    switch (eMessage) {
        case TOOMANYARGU:
            fprintf(stderr, "Error: too many process arguments\n");
            return;
        case NODIR:
            fprintf(stderr, "Error: no such directory\n");
            return;
        case NOCMD:
            fprintf(stderr, "Error: command not found\n");
            return;
        case MISSCMD:
            fprintf(stderr, "Error: missing command\n");
            return;
        case MISLOCATEI:
            fprintf(stderr, "Error: mislocated input redirection\n");
            return;
        case NOI:
            fprintf(stderr, "Error: no input file\n");
            return;
        case CANTOPENI:
            fprintf(stderr, "Error: cannot open input file\n");
            return;
        case MISLOCATEO:
            fprintf(stderr, "Error: mislocated output redirection\n");
            return;
        case NOO:
            fprintf(stderr, "Error: no output file\n");
            return;
        case CANTOPENO:
            fprintf(stderr, "Error: cannot open output file\n");
            return;
        case MISLOCATEB:
            fprintf(stderr, "Error: mislocated background sign\n");
            return;
        case ACTIVE:
            fprintf(stderr, "active jobs still running\n");
            return;
    }
}

// Check if the command input is in the commandlist
bool check_error(char* cmd) {
    for (int i = 0; i < CMDNUM; i++) {
        if (!strcmp(cmd, CmdList[i])) {
            return false;
        }
    }
    return true;
}

// Open the file when input or output redirection
// open() source:
// https://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
void file_redirection(char* filename, bool innout) {
    int fd;
    fd = innout ? open(filename, O_RDONLY) : open(filename,
        O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    // check if open() failed
    if (fd == -1) {
        if (innout) {
            print_error(CANTOPENI);
            memset(filename, 0, MAXFILENAME);
            exit(1);
        } else {
            print_error(CANTOPENO);
            memset(filename, 0, MAXFILENAME);
            exit(1);
        }
    }
    if (innout) {
        dup2(fd, STDIN_FILENO);
    } else {
        dup2(fd, STDOUT_FILENO);
    }
    close(fd);
    memset(filename, 0, MAXFILENAME);
}

// File redirection or wait for further input
// setpgid() source:
// https://www.gnu.org/software/libc/manual/html_mono/libc.html#Process-Completion
void apply_message(Command* commands, int SplitNumber) {
    if (commands->Message == INPUT) {
        file_redirection(commands->filename, true);
    }
    if (commands[SplitNumber].Message == OUTPUT) {
        file_redirection(commands[SplitNumber].filename, false);
    }
    // Encounter &
    if (commands->Message == AMPERSAND) {
        setpgid(0, 0);
    }
}

// Update filename with input command
void insert_filename(Command* commands, char* cmd, int* StorePostion) {
    int it = 0;
    if ((cmd[*StorePostion] != '<') && (cmd[*StorePostion] != '>'))  {
        return;
    }
    commands->Message = cmd[*StorePostion] == '<' ? INPUT : OUTPUT;
    // Omit whitespaces
    do {
        (*StorePostion)++;
    } while (cmd[*StorePostion] == ' ');
    // Check if there is no input or output filename
    if (cmd[*StorePostion] == '\n') {
        if (commands->Message == INPUT) {
            print_error(NOI);
            exit(1);
        } else {
            print_error(NOO);
            exit(1);
        }
    }
    // Insert filename
    while (cmd[*StorePostion] != ' ' && cmd[*StorePostion] != '\n') {
        (commands->filename)[it] = cmd[*StorePostion];
        it++;
        (*StorePostion)++;
    }
}

// Check some error inputs
bool check_locate(Command* commands, int SplitNumber) {
    // There is only a '&'
    if ((commands->Message == AMPERSAND) && (commands->argc == 0)) {
        print_error(MISSCMD);
        return true;
    }
    if (SplitNumber > 0) {
        // There is no arguments in a certain pipe
        for (int i = 0; i <= SplitNumber; i++) {
            if (commands[i].argc == 0) {
                print_error(MISSCMD);
                return true;
            }
        }
        // mislocate <
        if (commands[SplitNumber].Message == INPUT) {
            print_error(MISLOCATEI);
            return true;
        }
        // mislocate >
        if (commands->Message == OUTPUT) {
            print_error(MISLOCATEO);
            return true;
        }
        // mislocate &
        if (commands->Message == AMPERSAND) {
            print_error(MISLOCATEB);
            return true;
        }
    }
    return false;
}

// Check if the input has | < > &
bool check_message(char InputChar, Command* commands,
    int* SplitNumber, int* argc) {
    switch (InputChar) {
        case '|':
            // Pipe and add SplitNumber by 1
            // Update commands[0] to commands[1]
            (commands[*SplitNumber].process)[*argc] = NULL;
            commands[*SplitNumber].argc = *argc;
            (*SplitNumber)++;
            commands[*SplitNumber].Message = NORMAL;
            *argc = 0;
            return true;
        case '<':
            commands[*SplitNumber].Message = INPUT;
            return true;
        case '>':
            commands[*SplitNumber].Message = OUTPUT;
            return true;
        case '&':
            commands[*SplitNumber].Message = AMPERSAND;
            return true;
    }
    return false;
}

// Get the command input and assign values into commands
void get_command(int* SplitNumber, char* cmd, Command* commands) {
    char temp[MAXINPUT];
    int InputPostion = 0;
    int StorePostion = 0;
    int argc = 0;
    // Get command line
    fgets(cmd, MAXINPUT, stdin);
    while (cmd[StorePostion] != '\0') {
        temp[InputPostion] = cmd[StorePostion];
        StorePostion++;
        // Check if it is | < > &
        if (check_message(temp[InputPostion], commands, SplitNumber, &argc)) {
            continue;
        }
        // Check if it is whitespace or finished
        if ((temp[InputPostion] != ' ') && (temp[InputPostion] != '\n')) {
            InputPostion++;
        } else if (InputPostion) {
            temp[InputPostion] = '\0';
            (commands[*SplitNumber].process)[argc] =
                malloc(strlen(temp) * sizeof(char));
            // Assign input command into command process string
            strcpy((commands[*SplitNumber].process)[argc], temp);
            argc++;
            memset(temp, 0, MAXINPUT);
            InputPostion = 0;
            insert_filename(&commands[*SplitNumber], cmd, &StorePostion);
        }
    }
    // Finish get_command() and update argc
    (commands[*SplitNumber].process)[argc] = NULL;
    commands[*SplitNumber].argc = argc;
}

// Check if it is a Builtin Command
bool check_builtin(Command* commands) {
    // exit
    if (!strcmp((commands->process)[0], "exit")) {
        // check if it has & before
        if (commands->Message == AMPERSAND) {
            print_error(ACTIVE);
        }
        fprintf(stderr, "Bye...\n");
        exit(1);
    }
    // cd
    if (!strcmp((commands->process)[0], "cd")) {
        if (chdir((commands->process)[1]) == -1) {
            print_error(NODIR);
        }
        return true;
    }
    // pwd
    if (!strcmp((commands->process)[0], "pwd")) {
        char cwd[MAXCWD];
        getcwd(cwd, sizeof(cwd));
        fprintf(stdout, "%s\n", cwd);
        return true;
    }
    return false;
}

// Display "sshell$"
void display_prompt() {
    fprintf(stdout, "sshell$ ");
}

// Pipeline using dup2()
void pipe_line(Command* commands) {
    int fd[2];
    pipe(fd);
    if (fork() != 0) {  // parent uses STDIN_FILENO
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
    } else {  // child uses STDOUT_FILENO
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execvp((commands->process)[0], commands->process);
    }
}

// Pipe mutiple commands and execute them, wait them in the end
void split_command(int SplitNumber, int* status, Command* commands) {
    int iteration;
    // First execute then wait
    for (iteration = 0; iteration < SplitNumber; iteration++) {
        pipe_line(&commands[iteration]);
    }
    for (iteration = 0; iteration < SplitNumber; iteration++) {
        wait(&status[iteration]);
    }
}

// Finish the command and print the exit number
// Print exit numbers if pipe
void print_complete(int* status, char* cmd, int SplitNumber) {
    fprintf(stderr, "+ completed '%s' ", cmd);
    for (int i = 0; i <= SplitNumber; i++) {
        // print all exit numbers
        fprintf(stderr, "[%d]", WEXITSTATUS(status[i]));
    }
    fprintf(stderr, "\n");
}

// Use free() to free the memory malloced
void free_command(Command* commands, int SplitNumber) {
    for (int i = 0; i < SplitNumber; i++) {
        for (int j = 0; j < commands[i].argc; j++) {
            free(commands[i].process[j]);
        }
    }
}

// Renew the variables for next command
void reset_args(char* cmd, Command* commands, int* status) {
    memset(cmd, '\0', MAXINPUT * sizeof(char));
    memset(commands, 0, MAXCMD * sizeof(Command));
    memset(status, 0, MAXPIPE * sizeof(int));
}

// Main function
void do_command() {
    Command commands[MAXCMD];
    pid_t pid;
    char cmd[MAXINPUT];
    char *nl;
    int status[MAXPIPE];
    int SplitNumber;
    while (true) {
        SplitNumber = 0;
        commands->Message = NORMAL;
        display_prompt();
        fflush(stdout);
        get_command(&SplitNumber, cmd, commands);
        // Print command line if we're not getting stdin from the terminal
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }
        // Remove trailing newline from command line
        nl = strchr(cmd, '\n');
        if (nl) {
            *nl = '\0';
        }
        // Check "mislocate" and "missing command" errors
        if (check_locate(commands, SplitNumber)) {
            continue;
        }
        // Check "too many arguments" error
        if (commands->argc > 16) {
            print_error(TOOMANYARGU);
            continue;
        }
        // Builtin command
        if (check_builtin(commands)) {
            print_complete(status, cmd, SplitNumber);
            reset_args(cmd, commands, status);
            continue;
        }
        if ((pid = fork()) > 0) {  // parent
            if (commands[SplitNumber].Message != AMPERSAND) {
                waitpid(-1, status, 0);
            }
        } else if (pid == 0) {  // child
            if (check_error((commands->process)[0])) {
                print_error(NOCMD);
                exit(1);
            }
            // Redirection < > or wait &
            apply_message(commands, SplitNumber);
            if (SplitNumber) {
                // Pipe |
                split_command(SplitNumber, status, commands);
            }
            // Execute
            execvp((commands[SplitNumber].process)[0],
                (commands[SplitNumber].process));
            // Execute failed
            exit(1);
        } else {
            // Fork failed
            perror("fork");
            exit(1);
        }
        // Free memory, print completion, and reset arguments
        free_command(commands, SplitNumber);
        print_complete(status, cmd, SplitNumber);
        reset_args(cmd, commands, status);
    }
}

int main() {
    do_command();
    return EXIT_SUCCESS;
}

