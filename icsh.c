/* ICCS227: Project 1: icsh
 * Name: Krit Jarupanitkul
 * StudentID: 6480604
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CMD_BUFFER 255

/*
Milestone 1: Interactive command-line interpreter
*/

void init_shell() {
    printf("Starting IC shell\n");
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

void process_command(char *buffer, char *last) {
    // 3. History (!!)
    if (handle_history(buffer, last) == -1) {
        // History command was executed, no further processing needed
        return;
    }

    // 4. Built-ins
    if (strncmp(buffer, "echo ", 5) == 0) {
        handle_echo(buffer);
    } else if (strncmp(buffer, "exit", 4) == 0) {
        exit(handle_exit(buffer));
    } else {
        handle_bad_command();
    }
}

int main() {
    char buffer[MAX_CMD_BUFFER];
    char last[MAX_CMD_BUFFER] = "";
    int exit_code = 0;

    init_shell();

    while (1) {
        // 1. Prompt
        prompt();
        if (!fgets(buffer, MAX_CMD_BUFFER, stdin))
            break; // EOF (Ctrl-D)

        // 2. Strip newline
        strip_newline(buffer);
        if (buffer[0] == '\0')
            continue; // Empty line

        // Process the command
        process_command(buffer, last);
    }

    return exit_code;
}
