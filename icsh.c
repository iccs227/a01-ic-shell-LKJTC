/* ICCS227: Project 1: icsh
 * Name: Krit Jarupanitkul
 * StudentID: 6480604
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#define MAX_CMD_BUFFER 255
#define MAX_TOKENS 64

volatile sig_atomic_t foreground_pid = 0;
int last_exit_status = 0;

/*
Milestone 1: Interactive command-line interpreter
*/

void init_shell() {
    printf("Welcome to the Interactive Command-line Shell (icsh)!\n");
    printf("Type 'exit' to quit or '!!' to repeat the last command.\n");
    printf("Type 'echo <message>' to print a message.\n");
    printf("Enjoy your stay!\n\n");
}

void prompt() {
    printf("icsh $ ");
}

void strip_newline(char *buffer) {
    buffer[strcspn(buffer, "\n")] = '\0';
}

int handle_history(char *buffer, char *last) {
    if (strcmp(buffer, "!!") == 0) {
        if (last[0] == '\0') {
            return -1;
        }
        printf("%s\n", last);
        strcpy(buffer, last);
        return 1;
    }
    strcpy(last, buffer);
    return 0;
}

void handle_echo(const char *buffer) {
    if (strncmp(buffer, "echo $?", 7) == 0) {
        printf("%d\n", last_exit_status);
    } else {
        printf("%s\n", buffer + 5);
    }
}

int handle_exit(const char *buffer) {
    int code = atoi(buffer + 4) & 0xFF;
    printf("bye\n");
    return code;
}

void handle_bad_command() {
    printf("bad command\n");
}

/*
Milestone 3: Running External Commands
*/
int split_args(char *line, char **tokens) {
    int ntok = 0;
    char *p = strtok(line, " \t");
    while (p && ntok < MAX_TOKENS - 1) {
        tokens[ntok++] = p;
        p = strtok(NULL, " \t");
    }
    tokens[ntok] = NULL;
    return ntok;
}

int run_external(char **tokens, int background) {
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        execvp(tokens[0], tokens);
        perror("execvp");
        exit(1);
    }
    int status = 0;
    if (!background) {
        foreground_pid = pid;
        waitpid(pid, &status, WUNTRACED);
        foreground_pid = 0;
        if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_exit_status = 128 + WTERMSIG(status);
        }
    }
    return status;
}

/*
Milestone 4: Signal Handlers
*/
void sigint_handler(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGINT);
    }
}

void sigtstp_handler(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGSTOP);
    }
}

void install_signal_handlers() {
    struct sigaction sa_int, sa_tstp;

    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);
}

void process_command(char *buffer, char *last) {
    if (handle_history(buffer, last) == -1) {
        return;
    }
    char *tokens[MAX_TOKENS];
    int ntok = split_args(buffer, tokens);

    if (ntok == 0) return;
    
    else if (strcmp(tokens[0], "echo") == 0) {
        // Handle `echo $?`
        for (int i = 1; i < ntok; ++i) {
            if (strcmp(tokens[i], "$?") == 0) {
                printf("%d", last_exit_status);
            } else {
                printf("%s", tokens[i]);
            }
            if (i != ntok - 1) printf(" ");
        }
        printf("\n");
        last_exit_status = 0;
    } else if (strcmp(tokens[0], "exit") == 0) {
        last_exit_status = 0;
        exit(handle_exit(buffer));
    } else {
        int status = run_external(tokens, 0);
        if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_exit_status = 128 + WTERMSIG(status);
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 1) {
            handle_bad_command();
        }
    }
}




int main(int argc, char *argv[]) {
    FILE *input = stdin;
    char buffer[MAX_CMD_BUFFER];
    char last[MAX_CMD_BUFFER] = "";
    int exit_code = 0;

    /*
    Milestone 2: Script mode
    */
    if (argc > 1) {
        input = fopen(argv[1], "r");
        if (!input) {
            perror("Failed to open script file");
            return 1;
        }
    }

    install_signal_handlers();
    init_shell();

    while (1) {
        if (input == stdin) {
            prompt();
        }
        if (!fgets(buffer, MAX_CMD_BUFFER, input))
            break;

        strip_newline(buffer);
        if (buffer[0] == '\0')
            continue;

        process_command(buffer, last);
    }

    if (input != stdin) {
        fclose(input);
    }

    return exit_code;
}
