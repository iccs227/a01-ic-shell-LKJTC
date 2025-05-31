/* ICCS227: Project 1: icsh
 * Name: Krit Jarupanitkul
 * StudentID: 6480604
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>  // for open
#include <unistd.h> // for dup2, close

#define MAX_CMD_BUFFER 255
#define MAX_TOKENS 64
#define MAX_JOBS 64

typedef enum { RUNNING, STOPPED, DONE } job_state;

typedef struct {
    int id;
    pid_t pid;
    job_state state;
    char cmdline[MAX_CMD_BUFFER];
} job_t;

job_t jobs[MAX_JOBS];
int next_job_id = 1;

volatile sig_atomic_t foreground_pid = 0; // PID of the current foreground process
int last_exit_status = 0;                 // Exit status of the last command

/*
Milestone 1: Interactive command-line interpreter
*/

// Print message when start shell
void init_shell() {
    printf("Welcome to the Interactive Command-line Shell (icsh)!\n");
    printf("Type 'exit' to quit or '!!' to repeat the last command.\n");
    printf("Type 'echo <message>' to print a message.\n");
    printf("Enjoy your stay!\n\n");
}

// shell prompt
void prompt() {
    printf("icsh $ ");
}

// Remove newline char 
void strip_newline(char *buffer) {
    buffer[strcspn(buffer, "\n")] = '\0';
}

// History !!
int handle_history(char *buffer, char *last) {
    if (strcmp(buffer, "!!") == 0) {
        if (last[0] == '\0') {
            return -1; // No previous
        }
        printf("%s\n", last); // Show last command
        strcpy(buffer, last); // Replace buf with last command
        return 1;
    }
    strcpy(last, buffer); // Save current command as last
    return 0;
}

// echo command
void handle_echo(const char *buffer) {
    if (strncmp(buffer, "echo $?", 7) == 0) {
        printf("%d\n", last_exit_status); // Print exit status
    } else {
        printf("%s\n", buffer + 5); // Print message after echo
    }
}

//Exit 
int handle_exit(const char *buffer) {
    int code = atoi(buffer + 4) & 0xFF; // Get exit code
    printf("bye\n");
    return code;
}

// Print error
void handle_bad_command() {
    printf("bad command\n");
}

/*
Milestone 6: Job Control
*/
int add_job(pid_t pid, job_state state, const char *cmdline) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].id = next_job_id++;
            jobs[i].pid = pid;
            jobs[i].state = state;
            strncpy(jobs[i].cmdline, cmdline, MAX_CMD_BUFFER-1);
            jobs[i].cmdline[MAX_CMD_BUFFER-1] = '\0';
            return jobs[i].id;
        }
    }
    return -1;
}

job_t* find_job_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].pid == pid) return &jobs[i];
    return NULL;
}

job_t* find_job_by_id(int id) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].id == id) return &jobs[i];
    return NULL;
}

void remove_job(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].pid == pid) jobs[i].pid = 0;
}

/*
Milestone 3: Running External Commands
*/

// Split input line into words
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

//Run an external command
int run_external(char **tokens, int background, char *infile, char *outfile, const char *full_cmd) {
    pid_t pid = fork();
    
    if (pid == 0) {
        setpgid(0, 0);

        // Milestone 5: I/O Redirection with dup2 and open()
        if (infile) {
            int fd_in = open(infile, O_RDONLY);
            if (fd_in < 0) {
                perror("open input file");
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO);  // Redirect stdin to fd_in
            close(fd_in);               // Close fd_in 
        }
        if (outfile) {
            int fd_out = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (fd_out < 0) {
                perror("open output file");
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO); // Redirect stdout to fd_out
            close(fd_out);               // Close fd_out 
        }

        execvp(tokens[0], tokens);
        perror("execvp");
        exit(1);
    }
    if (background && pid > 0) {
        setpgid(pid, pid);
        int job_id = add_job(pid, RUNNING, full_cmd);
        printf("[%d] %d\n", job_id, pid);
        fflush(stdout);
        return 0;
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
// Ctrl - C
void sigint_handler(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGINT);
    }
}

// Ctrl - Z
void sigtstp_handler(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGSTOP);
    }
}

void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = find_job_by_pid(pid);
        if (job) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                job->state = DONE;
                printf("\n[%d]+  Done\t\t%s\n", job->id, job->cmdline);
                remove_job(pid);
                prompt();
                fflush(stdout);
            } else if (WIFSTOPPED(status)) {
                job->state = STOPPED;
                printf("\n[%d]+  Stopped\t\t%s\n", job->id, job->cmdline);
                prompt();
                fflush(stdout);
            } else if (WIFCONTINUED(status)) {
                job->state = RUNNING;
            }
        }
    }
}

// Install signal handlers for SIGINT and SIGTSTP
// Check if a child process is running and send the signal to it, not to the shell.
void install_signal_handlers() {
    struct sigaction sa_int, sa_tstp, sa_chld;

    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa_chld, NULL);
}

void process_command(char *buffer, char *last) {
    char full_cmd[MAX_CMD_BUFFER];
    strncpy(full_cmd, buffer, MAX_CMD_BUFFER-1);
    full_cmd[MAX_CMD_BUFFER-1] = '\0';
    if (handle_history(buffer, last) == -1) {
        return;
    }
    char *tokens[MAX_TOKENS];
    int ntok = split_args(buffer, tokens);

    // === BEGIN: Milestone 5 Redirection Parsing ===
    char *infile = NULL, *outfile = NULL;
    int i = 0;
    while (i < ntok) {
        if (strcmp(tokens[i], "<") == 0 && i + 1 < ntok) {
            infile = tokens[i + 1];
            // Remove < and filename from tokens
            int j = i;
            while (j + 2 < ntok) {
                tokens[j] = tokens[j + 2];
                j++;
            }
            ntok -= 2;
            tokens[ntok] = NULL;
            continue;
        } else if (strcmp(tokens[i], ">") == 0 && i + 1 < ntok) {
            outfile = tokens[i + 1];
            int j = i;
            while (j + 2 < ntok) {
                tokens[j] = tokens[j + 2];
                j++;
            }
            ntok -= 2;
            tokens[ntok] = NULL;
            continue;
        }
        i++;
    }
    
    // Milestone 6: Background Job Detection 
    int background = 0;
    if (ntok > 0 && strcmp(tokens[ntok-1], "&") == 0) {
        background = 1;
        tokens[--ntok] = NULL; // Remove "&" from tokens
    }

    if (ntok == 0) return;

    // jobs built-in
    if (strcmp(tokens[0], "jobs") == 0) {
        for (int j = 0; j < MAX_JOBS; j++) {
            if (jobs[j].pid > 0) {
                printf("[%d] %s\t\t%s\n", jobs[j].id,
                    jobs[j].state == RUNNING ? "Running" :
                    jobs[j].state == STOPPED ? "Stopped" : "Done",
                    jobs[j].cmdline);
            }
        }
        return;
    }

    // fg built-in
    else if (strcmp(tokens[0], "fg") == 0 && tokens[1] && tokens[1][0] == '%') {
        int job_id = atoi(tokens[1] + 1);
        job_t *job = find_job_by_id(job_id);
        if (job && job->pid > 0) {
            job->state = RUNNING;
            printf("%s\n", job->cmdline);
            kill(job->pid, SIGCONT);
            foreground_pid = job->pid;
            waitpid(job->pid, NULL, WUNTRACED);
            foreground_pid = 0;
        }
        return;
    }

    // bg built-in
    else if (strcmp(tokens[0], "bg") == 0 && tokens[1] && tokens[1][0] == '%') {
        int job_id = atoi(tokens[1] + 1);
        job_t *job = find_job_by_id(job_id);
        if (job && job->pid > 0) {
            job->state = RUNNING;
            kill(job->pid, SIGCONT);
            printf("[%d]+ %s\n", job->id, job->cmdline);
        }
        return;
    }
    
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
        
        // --- PASS background TO run_external ---
        int status = run_external(tokens, background, infile, outfile, full_cmd);
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
