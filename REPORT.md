# Project 1-Shell

## Introduction
Our porject is to implement a shell. We wrote mutiple functions and used child
process to do the basic work of the shell with C language.

## Process
This project is arduous and complicated. We created struct command to store
values, and it is beneficial for us to store different values, command input
(char\*\*), filename(char\*), number of arguments(int), and signal message
(Signal), in one object. 
We first implemented the input getting. Inheriting sshell\_tester.c, the non-
input problems can be fixed (When the user only pushes "enter"), but there were
issues with < > | & signs. To avoid segmentation fault when reading inputs, we
read the inputs character by character. Thus, in get\_command() function, there
is a large while loop to read the command input character by character. We used
check\_message() to check if there is < > | & sign. If exists, we save message
to command.Message and ignore it in input. Another difficulty is to store the
filename when reading. We realized it by always check the next command input
character, if there is < or >, we change the Message and start reading filename.

The second part is to use parent and child process, using fork(), wait()
methods. Based on slides in lecture, we used pid to store the pid\_t value of
fork(). Then, if it is the parent process, we check if it is & AMPERSAND, & will
result a wait and do not execute the child process. Unless it will execute the
command via execvp(). The arguments are found in command.process. For parent
process, it will wait the child process finish, and do another command for the
next command input. There is a problem for child process, which is we cannot
handle pipe commands.

So the third part is to finish the pipe commands. As said before, we created
command as a struct. Then we set commands to be struct arrays. This makes
commands can be split when it encounters pipe (|), by just change commands[0] to
commands[1] and assign new values in commands[1]. Plus, we also make status to
be array of int, in order to save different exit status numbers. In child
process, we check the SplitNumber to see if it has pipe. If there is pipe, we
call function split\_command() to pipe mutiple commands and execute them in the
input order, then wait them finish in the input order as well. To execute these
commands, we used pipe\_line() command which is written based on lecture slides.
By using dup2(), we change the input and output stream. In this way, we finish
the pipe commands.

The fourth part is input and output redirection as well as AMPERSAND. In
fucntion get\_command(), we have already saved Message(Signal) into commands.
Therefore, we used function apply\_message() to apply implementation to < > &.
If it is AMPERSAND &, we use setpgid(), which is found in the project prompt.
For input and output redirection, we use dup2() to change the input or output
stream, just as utilized above. Then, we reset the filename for next command
input.

The fifth part of this project is the error management, it seems to be easy but
needs many time debugs. We initialize a enum type named Error. Once there is an
error, we will throw it into print\_error() function with ErrorMessage(Error)
and exit. After getting inputs, we first check the number of arguments greater
than 16 or not. Then we check the "mislocate" problems and "missing command"
problems, since if we check it later, it will cause Segmentation Fault during
future process. Also, before exit function, if Message is AMPERSAND, it will
send an ACTIVE ErrorMessage. In child process, before executing the child
process, we check if the command is one of the commandlist or not. Moreover, it
will send open file error message in the fragment of input or output
redirection, which can be found in file\_direction(). Lastly, for fork failed,
which refers to pid number is -1, we simply use perror() to report error.

To organize all these five parts, we use do\_command() as the main function, and
we wrote a while true loop inside. We initialize all variables including struct
command array first, and read input commands, check errors of the input. Then,
we check if it is builtin commands, if so, it will execute manually. Next, we
use fork() and wait() to create child process to run the execvp(). When pipe, it
enters into split\_command(). When AMPERSAND, it will not execute the command.
After the execution part, we free the memory allocated, print the completion,
and finally reset the variables for next command, AKA next loop.

## Test
To test our project, we employ the test\_sshell\_student.sh file to see if it
passes all phases. For error management, we create some error input command, and
see if it can correctly print the error messages and exit. To debug our project,
since it is hard for us to debug setting breakpoints with mutiple processes, we
use printf() to find out what happens when it reaches breakpoints.

## Future Improvments
The input reading part is character by characer, and it will always determine if
the next input character is < or >. Additionally, we have tried strtok(), but it
fails.  These implementation should be made in only one step for improvement.
The error management in our code is here and there, it should be more focus and
be detected in fewer functions. Plus, if there is combination of errors, the
order of error detection should be improved.

## list of Implements
* enum Error() 

A type to store Error Message.
* enum Signal() 

A type to store encountering < > & |.
* struct command() 

The main struct, including command input(char\*\*), filename(char\*), number of
arguments(int), and signal message(Signal).
* do\_command()

The main function, to store inputs, check errors, execute commands, and print
completion.
* get\_command(int\*, char\*, Command\*)

Get the command input and assign values into commands.
* check\_message(char, Command\*, int\*, int\*)

Check if there is < > & |.
* insert\_filename(Command\*, char\*, int\*)

When there is input or output redirection, assign filename from input to command.
* display\_prompt()

Print "sshell$".
* check\_locate(Command\*, int)

Check if there is "mislocate" or "missing command" errors.
* check\_builtin(Command\*)

Execute cd, pwd, or exit command not using execvp().
* check\_error(char\*)

Check if the command input is on the CommandList.
* apply\_message(Command\*, int)

File redirection < > or Wait for next command &.
* file\_redirection(char\*, bool)

Open the file when input or output redirection. If failed, throw it into
print\_error().
* split\_command(int, int\*, Command\*)

Pipe commands |, execute them in input order and wait them also in input order.
* pipe\_line(Command\*)

Based on Professor's given code. Use dup2() to change input or output stream.
* print\_error(Error)

Print errors to STDERR with Error Message.
* print\_complete(int\*, char\*, int)

Finish the command and print exit number, print mutiple exit numbers if pipe.
* reset\_args(char\*, Command\*, int\*)

Reset command input, command struct, status numbers preparing for next command.
* free\_command(Command\*, int)

Free the memory allocated in get\_command() function.

