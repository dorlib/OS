#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

// Error messages
#define COMMAND_NOT_FOUND_ERROR "Error!: failed to execute the command or command does not exist.\n"
#define FORK_ERROR "Error!: Failed to fork.\n"
#define PARENT_ERROR "Error!: error in parent process\n"

// Function prototypes
int process_arglist(int count, char **arglist);
int handle_background(int count, char **arglist);
int handle_pipe(int count, char **arglist);
int handle_redirect(int count, char **arglist);
int handle_append(int count, char **arglist);
int general_handler(int count, char **arglist);
int set_signal(int signum, void (*handler)(int));
void sigint_handler(int signum);
void sigchld_handler(int signum);
int controller(int count, char **arglist);

// Prepare the signal handling
int prepare() {
    // Set SIGINT to custom handler (print newline)
    if (set_signal(SIGINT, sigint_handler) != 0) {
        return 1;
    }

    // Set SIGCHLD to custom handler (reap child processes)
    if (set_signal(SIGCHLD, sigchld_handler) != 0) {
        return 1;
    }

    return 0;
}

// Signal handler function for SIGCHLD to reap child processes
void sigchld_handler(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Signal handler function for SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    // Print newline to separate from previous command output
    printf("\n");
    fflush(stdout);
}

// Helper function to set signal handling
int set_signal(int signum, void (*handler)(int)) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(signum, &sa, NULL) == -1) {
        fprintf(stderr, "Error!: Failed to set signal handler for signal %d\n", signum);
        return 1;
    }

    return 0;
}

// Process the argument list and determine the correct handler
int process_arglist(int count, char **arglist) {
    if (count <= 0) {
        fprintf(stderr, "Error! Command not given\n");
        return 0;
    }

    int handler = controller(count, arglist);

    // Call the appropriate handler based on the detected command type
    switch (handler) {
        case 1:
            return handle_background(count, arglist);
        case 2:
            return handle_pipe(count, arglist);
        case 3:
            return handle_redirect(count, arglist);
        case 4:
            return handle_append(count, arglist);
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
        signal(SIGINT, SIG_DFL); // Child inherits default SIGINT handling
        signal(SIGCHLD, SIG_DFL); // Child inherits default SIGCHLD handling

        if (execvp(arglist[0], arglist) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            exit(1); // Exit with error status
        }
    }

    int status;
    if (waitpid(pid, &status, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, PARENT_ERROR);
        return 0;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Handle appending redirected output
int handle_append(int count, char **arglist) {
    int append_index;
    for (append_index = 0; append_index < count; append_index++) {
        if (strcmp(arglist[append_index], ">>") == 0) {
            break;
        }
    }

    if (append_index == count || append_index == count - 1) {
        fprintf(stderr, "Error!: No file specified for append\n");
        return 0;
    }

    char *file = arglist[append_index + 1];
    arglist[append_index] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid == 0) {
        signal(SIGINT, SIG_DFL); // Child inherits default SIGINT handling

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

    int status;
    if (waitpid(pid, &status, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, PARENT_ERROR);
        return 0;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Handle input redirection
int handle_redirect(int count, char **arglist) {
    int redirect_index;
    for (redirect_index = 0; redirect_index < count; redirect_index++) {
        if (strcmp(arglist[redirect_index], "<") == 0) {
            break;
        }
    }

    if (redirect_index == count || redirect_index == count - 1) {
        fprintf(stderr, "Error!: No file specified for input redirection\n");
        return 0;
    }

    char *file = arglist[redirect_index + 1];
    arglist[redirect_index] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid == 0) {
        signal(SIGINT, SIG_DFL); // Child inherits default SIGINT handling

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

    int status;
    if (waitpid(pid, &status, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, PARENT_ERROR);
        return 0;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Handle piping between two commands
int handle_pipe(int count, char **arglist) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        fprintf(stderr, "Error!: failed to create pipe\n");
        return 0;
    }

    int pipe_index;
    for (pipe_index = 0; pipe_index < count; pipe_index++) {
        if (strcmp(arglist[pipe_index], "|") == 0) {
            break;
        }
    }

    if (pipe_index == count || pipe_index == 0 || pipe_index == count - 1) {
        fprintf(stderr, "Error!: Incorrect usage of pipe\n");
        return 0;
    }

    arglist[pipe_index] = NULL;

    pid_t pid1 = fork();
    if (pid1 < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid1 == 0) {
        signal(SIGINT, SIG_DFL); // Child inherits default SIGINT handling

        close(pipe_fd[0]);
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "Error!: failed to redirect stdout\n");
            exit(1);
        }

        close(pipe_fd[1]);

        if (execvp(arglist[0], arglist) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            exit(1);
        }
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid2 == 0) {
        signal(SIGINT, SIG_DFL); // Child inherits default SIGINT handling

        close(pipe_fd[1]);
        if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
            fprintf(stderr, "Error!: failed to redirect stdin\n");
            exit(1);
        }

        close(pipe_fd[0]);

        if (execvp(arglist[pipe_index + 1], &arglist[pipe_index + 1]) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            exit(1);
        }
    }

    close(pipe_fd[0]);
    close(pipe_fd[1]);

    int status;
    if (waitpid(pid1, &status, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, PARENT_ERROR);
        return 0;
    }

    if (waitpid(pid2, &status, 0) < 0 && errno != EINTR && errno != ECHILD) {
        fprintf(stderr, PARENT_ERROR);
        return 0;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Handle background processes
int handle_background(int count, char **arglist) {
    int background_index;
    for (background_index = 0; background_index < count; background_index++) {
        if (strcmp(arglist[background_index], "&") == 0) {
            break;
        }
    }

    if (background_index == count) {
        fprintf(stderr, "Error!: Background operator '&' not found\n");
        return 0;
    }

    arglist[background_index] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, FORK_ERROR);
        return 0;
    }

    if (pid == 0) {
        signal(SIGINT, SIG_IGN); // Ignore SIGINT in background processes
        signal(SIGCHLD, SIG_DFL); // Child inherits default SIGCHLD handling

        if (execvp(arglist[0], arglist) < 0) {
            fprintf(stderr, COMMAND_NOT_FOUND_ERROR);
            exit(1);
        }
    }

    // Parent process does not wait for background process to finish
    return 1;
}

// Controller function to detect command type
int controller(int count, char **arglist) {
    for (int i = 0; i < count; i++) {
        if (strcmp(arglist[i], "&") == 0) {
            return 1; // Background
        }
        if (strcmp(arglist[i], "|") == 0) {
            return 2; // Pipe
        }
        if (strcmp(arglist[i], "<") == 0) {
            return 3; // Input Redirection
        }
        if (strcmp(arglist[i], ">>") == 0) {
            return 4; // Output Redirection (Append)
        }
    }
    return 0; // Default (Normal Command)
}

int finalize() {
    int status;
    pid_t pid;

    // Wait for any remaining child processes to terminate
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child %d was terminated by signal %d\n", pid, WTERMSIG(status));
        }
    }

    if (pid == -1 && errno != ECHILD) {
        fprintf(stderr, "Error!: waitpid failed\n");
        return 1;
    }

    return 0;
}