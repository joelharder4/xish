#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "xish_state.h"

typedef struct {
    pid_t pid;
    int job_id;
} ChildProcess;

static ChildProcess *child_processes = NULL;
static int num_children = 0;
static int next_job_id = 1;


typedef struct ShellVar {
    char *name;
    char *value;
} ShellVar;

static ShellVar *shell_vars = NULL;
static int num_shell_vars = 0;


int get_num_children() {
    return num_children;
}


void add_child_pid(pid_t pid, int isBackground) {
    child_processes = realloc(child_processes, sizeof(ChildProcess) * (num_children + 1));
    child_processes[num_children].pid = pid;
    if (isBackground) {
        child_processes[num_children].job_id = next_job_id;
        next_job_id++;
    } else {
        // foreground process doesnt have a job id
        child_processes[num_children].job_id = -1;
    }
    num_children++;
}


void remove_child_pid(pid_t pid) {
    for (int i = 0; i < num_children; i++) {
        if (child_processes[i].pid == pid) {
            for (int j = i; j < num_children - 1; j++) {
                child_processes[j] = child_processes[j + 1];
            }
            num_children--;
            child_processes = realloc(child_processes, sizeof(ChildProcess) * num_children);
            return;
        }
    }
}



void cleanup_zombie_children() {
    for (int i = 0; i < num_children; i++) {
        int wstatus;
        int result = waitpid(child_processes[i].pid, &wstatus, WNOHANG);

        if (result > 0) {
            if (WIFEXITED(wstatus)) {
                int exitStatus = WEXITSTATUS(wstatus);
                if (exitStatus != 0) {
                    fprintf(stderr, "Child(%d) exited -- failure (%d)\n", child_processes[i].pid, exitStatus);
                } else {
                    fprintf(stderr, "Child(%d) exited -- success (%d)\n", child_processes[i].pid, exitStatus);
                }
            } else {
                fprintf(stderr, "Child(%d) crashed\n", child_processes[i].pid);
            }
            remove_child_pid(child_processes[i].pid);
            i--; // Adjust index after removal
        } else if (result < 0){
            remove_child_pid(child_processes[i].pid);
            i--;
        }
    }
}


// haha funny function name
void kill_children() {
    for (int i = 0; i < num_children; i++) {
        kill(child_processes[i].pid, SIGINT);
    }
    int exit_status;
    for (int i = 0; i < num_children; i++) {
        wait(&exit_status);
    }

    free(child_processes);
    child_processes = NULL;
    num_children = 0;
}



int get_job_id(pid_t pid) {
    for (int i = 0; i < num_children; i++) {
        if (child_processes[i].pid == pid) {
            return child_processes[i].job_id;
        }
    }
    return -1;
}



int get_pid(int job_id) {
    for (int i = 0; i < num_children; i++) {
        if (child_processes[i].job_id == job_id) {
            return child_processes[i].pid;
        }
    }
    return -1;
}






void add_shell_var(char* name, char* value) {
    if (strlen(name) == 0 || strlen(value) == 0) {
        fprintf(stderr, "Shell variable name and value cannot be empty\n");
        return;
    }

    // if variable is already declared, change its value
    for (int i = 0; i < num_shell_vars; i++) {
        if (strcmp(shell_vars[i].name, name) == 0) {
            free(shell_vars[i].value);
            shell_vars[i].value = strdup(value);
            return;
        }
    }

    // declare new variable
    shell_vars = realloc(shell_vars, sizeof(ShellVar) * (num_shell_vars + 1));
    shell_vars[num_shell_vars].name = strdup(name);
    shell_vars[num_shell_vars].value = strdup(value);
    num_shell_vars++;
}



void get_shell_var(char* name, char** value) {
    *value = NULL;  // Initialize to NULL
    for (int i = 0; i < num_shell_vars; i++) {
        if (strcmp(shell_vars[i].name, name) == 0) {
            *value = shell_vars[i].value;  // Just assign pointer, don't copy
            return;
        }
    }
}


void free_shell_vars() {
    for (int i = 0; i < num_shell_vars; i++) {
        free(shell_vars[i].name);
        free(shell_vars[i].value);
    }
    free(shell_vars);
    shell_vars = NULL;
    num_shell_vars = 0;
}