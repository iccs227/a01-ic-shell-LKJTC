/* ICCS227: Project 1: icsh
 * Name: Krit Jarupanitkul
 * StudentID: 6480604
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_CMD_BUFFER 255
#define MAX_TOKENS 64

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
            // user typed !! but there's no last command
            return -1;
        }
        // replay
        printf("%s\n", last);
        strcpy(buffer, last);
        return 1;
    }
    // other than (non-empty) command → record it
    strcpy(last, buffer);
    return 0;
}


void handle_echo(const char *buffer) {
    printf("%s\n", buffer + 5);
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
Milestone 3
*/
int tokenize(char *line, char **tokens) {
    int ntok = 0;
    char *p = strtok(line, " \t");
    while (p && ntok < MAX_TOKENS-1) {
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
        waitpid(pid, &status, 0);
    }
    return status;
}

void process_command(char *buffer, char *last) {
    char *tokens[MAX_TOKENS];
    int ntok = tokenize(buffer, tokens);

    if (ntok == 0) return; // No command
    //History !!
    if (handle_history(buffer, last) == -1) {
        return;
    }
    //Built-ins
    if (strncmp(buffer, "echo ", 5) == 0) {
        handle_echo(buffer);
    } else if (strncmp(buffer, "exit", 4) == 0) {
        exit(handle_exit(buffer));
    } else {
        int status = run_external(tokens, 0);
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
    Milestone 2 Script mode
    */
    if (argc > 1) {
        input = fopen(argv[1], "r");
        if (!input) {
            perror("Failed to open script file");
            return 1;
        }
    }

    init_shell();

    while (1) {
        // 1. Prompt
        if (input == stdin) {
            prompt();
        }
        if (!fgets(buffer, MAX_CMD_BUFFER, input))
            break; // EOF (Ctrl-D)

        // 2. Strip newline
        strip_newline(buffer);
        if (buffer[0] == '\0')
            continue; // Empty line

        // Process the command
        process_command(buffer, last);
    }
    if (input != stdin) {
        fclose(input);
    }

    return exit_code;
}
