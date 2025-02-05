#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define PIPE '|'
#define AMPERSAND '&'
#define REDIRECT '<'
#define APPEND ">>"

// Error messages
#define COMMAND_NOT_FOUND_ERROR "Error!: failed to execute the command or command does not exist.\n"
#define FORK_ERROR "Error!: Failed to fork.\n"
#define SIGACTION_ERROR "Error!: sigaction failed"

// Function prototypes
int controller(int count, char **pString);
int ampersand_handler(int count, char **pString);
int pipe_handler(int count, char **pString);
int redirect_handler(int count, char **pString);
int append_handler(int count, char **pString);
int general_handler(int count, char **pString);
int signal_handler();

int prepare(void) {
    struct sigaction sa1;
    sa1.sa_handler = SIG_IGN;
    sa1.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa1, NULL) == -1) {
        fprintf(stderr, SIGACTION_ERROR);
        return 1;
    }

    struct sigaction sa2;
    sa2.sa_handler = SIG_IGN;
    sa2.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa2, NULL) == -1) {
        fprintf(stderr, SIGACTION_ERROR);
        return 1;
    }

    return 0;
}

// Process the argument list and determine the correct handler
int process_arglist(int count, char **arglist) {
    if (count <= 0) {
        fprintf(stderr, "Error! command not given\n");
        return 0;
    }

    int handler = controller(count, arglist);

    // Call the appropriate handler based on the detected command type
    switch (handler) {
        case 1:
            return ampersand_handler(count, arglist);
        case 2:
            return pipe_handler(count, arglist);
        case 3:
            return redirect_handler(count, arglist);
        case 4:
            return append_handler(count, arglist);
        default:
            return general_handler(count, arglist);
    }
}

// General command handler
int general_handler(int count, char **arglist) {
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid == 0) {
        signal_handler();

        int status = execvp(arglist[0], arglist);
        if (status < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            exit(1);
        }
    }

    int status = waitpid(pid, NULL, 0);
    if (status < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, "Error!: error in parent\n");
        return 0;
    }

    return 1;
}

// Append handler for commands like `command >> file`
int append_handler(int count, char **arglist) {
    int pid;
    int status;
    int append_index;

    // Find the append operator index
    for (append_index = 0; append_index < count; append_index++) {
        if (strcmp(arglist[append_index], APPEND) == 0) {
            break;
        }
    }

    if (append_index == count || append_index == count - 1) {
        fprintf(stderr, "Error!: no file specified for append\n");
        return 0;
    }

    char *file = arglist[append_index + 1];
    arglist[append_index] = NULL;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid == 0) {
        signal_handler();

        int fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            fprintf(stderr, "Error: failed to open file for append\n");
            exit(1);
        }

        if (dup2(fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "Error: failed to redirect stdout\n");
            exit(1);
        }

        close(fd);

        if (execvp(arglist[0], arglist) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            exit(1);
        }
    }

    if (waitpid(pid, &status, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, "Error: error in parent process\n");
        return 0;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return 0;
    }
    return 1;
}

// Redirect handler for commands like `command < file`
int redirect_handler(int count, char **arglist) {
    int pid;
    int status;
    int redirect_index;

    // Find the redirect operator index
    for (redirect_index = 0; redirect_index < count; redirect_index++) {
        if (strcmp(arglist[redirect_index], "<") == 0) {
            break;
        }
    }

    if (redirect_index == count || redirect_index == count - 1) {
        fprintf(stderr, "Error!: no file specified for input redirection\n");
        return 0;
    }

    char *file = arglist[redirect_index + 1];
    arglist[redirect_index] = NULL;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid == 0) {
        signal_handler();

        int fd = open(file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Error!: failed to open file for input redirection\n");
            exit(1);
        }

        if (dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, "Error!: failed to redirect stdin\n");
            exit(1);
        }

        close(fd);

        if (execvp(arglist[0], arglist) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            exit(1);
        }
    }

    if (waitpid(pid, &status, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, "Error!: error in parent process\n");
        return 0;
    }

    return 1;
}

// Pipe handler for commands like `command1 | command2`
int pipe_handler(int count, char **arglist) {
    int pipe_file_descriptors[2];
    pid_t pid1, pid2;

    int pipe_index;
    // Find the pipe operator index
    for (pipe_index = 0; pipe_index < count; pipe_index++) {
        if (strcmp(arglist[pipe_index], "|") == 0) {
            break;
        }
    }

    if (pipe_index == count || pipe_index == 0 || pipe_index == count - 1) {
        fprintf(stderr, "Error!: incorrect usage of pipe\n");
        return 0;
    }

    arglist[pipe_index] = NULL;

    if (pipe(pipe_file_descriptors) == -1) {
        fprintf(stderr, "Error!: failed to create pipe\n");
        return 0;
    }

    pid1 = fork();
    if (pid1 < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid1 == 0) {
        signal_handler();

        close(pipe_file_descriptors[0]);
        if (dup2(pipe_file_descriptors[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "Error!: failed to redirect stdout\n");
            _exit(1); // Terminate child process
        }

        close(pipe_file_descriptors[1]);

        if (execvp(arglist[0], arglist) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            _exit(1); // Terminate child process
        }
    }

    pid2 = fork();
    if (pid2 < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid2 == 0) {
        signal_handler();

        close(pipe_file_descriptors[1]);
        if (dup2(pipe_file_descriptors[0], STDIN_FILENO) == -1) {
            fprintf(stderr, "Error!: failed to redirect stdin\n");
            _exit(1); // Terminate child process
        }

        close(pipe_file_descriptors[0]);

        if (execvp(arglist[pipe_index + 1], &arglist[pipe_index + 1]) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            _exit(1); // Terminate child process
        }
    }

    close(pipe_file_descriptors[0]);
    close(pipe_file_descriptors[1]);

    int status1, status2;
    if (waitpid(pid1, &status1, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, "Error!: error waiting for first child\n");
        return 0;
    }

    if (waitpid(pid2, &status2, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, "Error!: error waiting for second child\n");
        return 0;
    }

    return 1;
}

// Handles executing a command with '&' at the end
int ampersand_handler(int count, char **arglist) {
    pid_t pid;
    arglist[count - 1] = NULL; // Null-terminate the arglist

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid == 0) {
        if (execvp(arglist[0], arglist) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            exit(1);
        }
    }

    return 1; // Parent returns immediately, child runs in the background
}

int signal_handler() {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
	fprintf(stderr, "Error!: sigaction failed");
	exit(1);
    }

    return 0;
}

// Checks if a character exists in a string
int contains_char(const char* str, char c) {
    return strchr(str, c) != NULL;
}

// Checks if a substring exists in a string
int contains_string(const char* str, const char* substr) {
    return strstr(str, substr) != NULL;
}

// Determines the command execution control flow based on special characters
int controller(int count, char **arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        if (contains_char(arglist[i], AMPERSAND) && *arglist[count - 1] == AMPERSAND) {
            return 1; // Execute asynchronously with '&'
        } else if (contains_char(arglist[i], PIPE)) {
            return 2; // Pipe handling
        } else if (contains_char(arglist[i], REDIRECT) && count >= 2 && *arglist[count - 2] == REDIRECT && arglist[count - 2] != NULL) {
            return 3; // Input redirection handling
        } else if (contains_string(arglist[i], APPEND)) {
            return 4; // Output redirection (append mode) handling
        }
    }
    return 5; // Default case
}

int finalize() {
    return 0;	
}
