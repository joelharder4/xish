#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <glob.h>
#include <signal.h>

#include "xish_run.h"
#include "xish_tokenize.h"
#include "xish_state.h"


/**
 * Print a prompt if the input is coming from a TTY
 */
static void prompt(FILE *pfp, FILE *ifp)
{
	if (isatty(fileno(ifp))) {
		fputs(PROMPT_STRING, pfp);
	}
}


// returns 1 on success
// returns 0 on non-fatal failure (like wrong number of arguments)
// returns -1 on fatal failure (like fork failure)
int execSingleCommand(
	FILE *ofp,
	char ** const tokens,
	int nTokens,
	int isBackground,
	int verbosity,
	int readPipe[2],
	int writePipe[2]
 	)
{
	if (verbosity > 1) {
		fprintf(stderr, " + ");
		fprintfTokens(stderr, tokens, 1);
	}


	// if it is one of the built-in commands, do it now
	// CD
	if (strcmp(tokens[0], "cd") == 0) {
		if (nTokens < 2) {
			fprintf(stderr, "cd: no directory specified\n");
			return 0;
		}
		if (nTokens > 2) {
			fprintf(stderr, "cd: too many arguments\n");
			return 0;
		}

		if (chdir(tokens[1]) != 0) {
			fprintf(stderr, "cd: cannot change to directory '%s' : %s\n", tokens[1], strerror(errno));
			return 0;
		}
		return 1;

	// PWD
	} else if (strcmp(tokens[0], "pwd") == 0) {
		if (nTokens > 1) {
			fprintf(stderr, "pwd: too many arguments\n");
			return 0;
		}
		char curDir[4096];
		if (getcwd(curDir, sizeof(curDir)) == NULL) {
			fprintf(stderr, "pwd: cannot get current directory : %s\n", strerror(errno));
			return 0;
		}
		fprintf(ofp, "%s\n", curDir);
		return 1;
	
	// INTR
	} else if (strcmp(tokens[0], "intr") == 0) {
		if (nTokens == 1) {
			kill_children();
		} else if (nTokens == 2) {
			int job_id = atoi(tokens[1]);
			int pid = get_pid(job_id);
			if (pid == -1) {
				fprintf(stderr, "intr: no such job id %d\n", job_id);
				return 0;
			}
			kill(pid, SIGINT);
		} else {
			fprintf(stderr, "intr: too many arguments\n");
			return 0;
		}
		return 1;

	// EXIT
	} else if (strcmp(tokens[0], "exit") == 0) {
		if (nTokens > 1) {
			fprintf(stderr, "exit: too many arguments\n");
			return 0;
		}
		exit(0);
		return -1; // only reaches here if exit failed
	}


	if (verbosity > 1) {
		fprintf(stderr, "readPipe: %d,%d writePipe: %d,%d\n",
			readPipe[0], readPipe[1], writePipe[0], writePipe[1]);
	}

	// otherwise, fork and exec
	int pid = fork();

	if (pid < 0) {
		fprintf(stderr, "Failed to create a child process\n");
		return -1;
	
	} else if (pid == 0) {
		// CHILD //

		// pipes
		if (readPipe[0] != -1 && readPipe[1] != -1) {
			close(0); // stdin
			if (dup(readPipe[0]) != 0) {
				fprintf(stderr, "Failed to redirect stdin to pipe : %s\n", strerror(errno));
				return -1;
			}
			close(readPipe[0]);

			close(readPipe[1]); // unused write end
		}

		if (writePipe[0] != -1 && writePipe[1] != -1) {
			close(1); // stdout
			if (dup(writePipe[1]) != 1) {
				fprintf(stderr, "Failed to redirect stdout to pipe : %s\n", strerror(errno));
				return -1;
			}
			close(writePipe[1]);

			close(writePipe[0]); // unused read end
		}

		// Try execvp first (searches PATH)
        if (execvp(tokens[0], tokens) < 0) {
            // If that fails and the command doesn't contain '/', try current directory
            if (strchr(tokens[0], '/') == NULL) {
                char localPath[512];
                snprintf(localPath, sizeof(localPath), "./%s", tokens[0]);
                execv(localPath, tokens);
            }
        }

		// if it reaches here, exec failed
		fprintf(stderr, "Failed to exec command '%s' : %s\n", tokens[0], strerror(errno));
		exit(1);

	} else {
		// PARENT //
		add_child_pid(pid, isBackground);

		if (readPipe[0] != -1) close(readPipe[0]);
    	if (readPipe[1] != -1) close(readPipe[1]);

		if (isBackground) {
			fprintf(stderr, "[%d] background process %d\n", get_job_id(pid), pid);
			return 1;
		}

		int wstatus;
		waitpid(pid, &wstatus, 0);
		remove_child_pid(pid);

		if (WIFEXITED(wstatus)) {
			int exitStatus = WEXITSTATUS(wstatus);
			if (exitStatus != 0) {
				fprintf(stderr, "Child(%d) exited -- failure (%d)\n", pid, exitStatus);
			} else {
				fprintf(stderr, "Child(%d) exited -- success (%d)\n", pid, exitStatus);
			}
		} else {
			fprintf(stderr, "Child(%d) crashed\n", pid);
		}
		return 1;
	}

	// should never reach here
	exit(1);
}



/**
 * Actually do the work
 */
int execFullCommandLine(
		FILE *ofp,
		char ** const tokens,
		int nTokens,
		int verbosity)
{
	if (verbosity > 0) {
		fprintf(stderr, " + ");
		fprintfTokens(stderr, tokens, 1);
	}

	// replace any variable uses with values
	for (int i = 0; i < nTokens; i++) {
        char *token = tokens[i];
        char *start = strstr(token, "${");
        
        while (start != NULL) {
            char *end = strchr(start, '}');
            
            int varNameLen = end - start - 2;
            char varName[256];
            strncpy(varName, start + 2, varNameLen);
            varName[varNameLen] = '\0';
            
            char *varValue;
            get_shell_var(varName, &varValue);
            
            int beforeLen = start - token;
            int afterLen = strlen(end + 1);
            int valueLen = varValue ? strlen(varValue) : 0;
            
            char *newToken = malloc(beforeLen + valueLen + afterLen + 1);
            strncpy(newToken, token, beforeLen);
            newToken[beforeLen] = '\0';
            
            if (varValue) {
                strcat(newToken, varValue);
            }
            strcat(newToken, end + 1);
            
            tokens[i] = newToken;
            token = newToken;
            
            start = strstr(token, "${");
        }
    }


	if (verbosity > 1) {
		fprintf(stderr, "After variable substitution: ");
		fprintfTokens(stderr, tokens, 1);
	}


	// perform any globbing
	char *expandedTokens[MAXTOKENS];
	int expandedCount = 0;

	for (int i = 0; i < nTokens; i++) {
		glob_t globResult;
		int globStatus = glob(tokens[i], GLOB_NOCHECK, NULL, &globResult);
		
		if (globStatus == 0 && globResult.gl_pathc > 0) {
			if (i == 0) {
				// if its the command name then only take the first match
				expandedTokens[expandedCount] = strdup(globResult.gl_pathv[0]);
				expandedCount++;
			} else {
				for (int j = 0; j < globResult.gl_pathc; j++) {
					if (expandedCount < MAXTOKENS) {
						expandedTokens[expandedCount] = strdup(globResult.gl_pathv[j]);
						expandedCount++;
					}
				}
			}
			globfree(&globResult);
		} else {
			if (expandedCount < MAXTOKENS) {
				expandedTokens[expandedCount++] = strdup(tokens[i]);
			}
		}
	}

	// Replace original tokens with expanded ones
	for (int i = 0; i < expandedCount; i++) {
		tokens[i] = expandedTokens[i];
	}
	nTokens = expandedCount;
	tokens[nTokens] = NULL;


	if (verbosity > 1) {
		fprintf(stderr, "After globbing: ");
		fprintfTokens(stderr, tokens, 1);
	}
		

	// is this a variable assignment?
	if (nTokens >= 3 && strcmp(tokens[1], "=") == 0) {
		if (nTokens > 3) {
			fprintf(stderr, "Invalid variable assignment\n");
			return -1;
		}

		add_shell_var(tokens[0], tokens[2]);
		return 1;
	}


	int isBackground = 0;
	if (strcmp(tokens[nTokens - 1], "&") == 0 && nTokens > 1) {
	    isBackground = 1;
		tokens[nTokens - 1] = NULL;
	    nTokens--;
	}


	// split into commands by looking for pipes
	int pipefd1[2] = {-1, -1};
	int pipefd2[2] = {-1, -1};
	int i = 0;
	for (int j = 0; j <= nTokens; j++) {
		if (j == nTokens || strcmp(tokens[j], "|") == 0) {

			// if it is a pipe character
			if (j < nTokens) {
				tokens[j] = NULL;
				if (pipe(pipefd2) < 0) {
					fprintf(stderr, "Failed to create pipe : %s\n", strerror(errno));
					return -1;
				}
			} else {
				pipefd2[0] = -1;
				pipefd2[1] = -1;
			}

			if (verbosity > 0) {
				fprintf(stderr, "Executing command from token %d to %d\n", i, j - 1);
			}
			
			if (execSingleCommand(ofp, &tokens[i], j - i, isBackground, verbosity, pipefd1, pipefd2) < 0) {
				fprintf(stderr, "Failed executing command:\n    ");
				fprintfTokens(stderr, tokens, 1);
				return -1;
			}

			// write pipe of current command becomes the read pipe of the next command
			pipefd1[0] = pipefd2[0];
			pipefd1[1] = pipefd2[1];
			i = j + 1;
		}
	}

	return 1;
}

/**
 * Load each line and perform the work for it
 */
int
runScript(
		FILE *ofp, FILE *pfp, FILE *ifp,
		const char *filename, int verbosity
	)
{
	char linebuf[LINEBUFFERSIZE];
	char *tokens[MAXTOKENS];
	int lineNo = 1;
	int nTokens, executeStatus = 0;

	fprintf(stderr, "SHELL PID %ld\n", (long) getpid());

	prompt(pfp, ifp);
	while ((nTokens = parseLine(ifp,
				tokens, MAXTOKENS,
				linebuf, LINEBUFFERSIZE, verbosity - 3)) > 0) {
		lineNo++;

		if (nTokens > 0) {

			executeStatus = execFullCommandLine(ofp, tokens, nTokens, verbosity);

			if (executeStatus < 0) {
				fprintf(stderr, "Failure executing '%s' line %d:\n    ",
						filename, lineNo);
				fprintfTokens(stderr, tokens, 1);
				return executeStatus;
			}
		}
		
		// check for any child processes that have ended
		cleanup_zombie_children(ofp);
		
		prompt(pfp, ifp);
	}

	return (0);
}


/**
 * Open a file and run it as a script
 */
int
runScriptFile(FILE *ofp, FILE *pfp, const char *filename, int verbosity)
{
	FILE *ifp;
	int status;

	ifp = fopen(filename, "r");
	if (ifp == NULL) {
		fprintf(stderr, "Cannot open input script '%s' : %s\n",
				filename, strerror(errno));
		return -1;
	}

	status = runScript(ofp, pfp, ifp, filename, verbosity);
	fclose(ifp);
	return status;
}

