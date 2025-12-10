# CIS*3050 A1

By Joel Harder

## Testing Framework

You can run tests with `./runtests` and optionally add `-v` if you want to see the diff for any tests that failed.


## Variable Handling and Management
all variables are managed through the `xish_state.c` (and header) file. It is a simple struct that has name and value strings. The functions `add_shell_var()`, `get_shell_var()`, and `free_shell_vars()` are used in order to manage adding, getting, and removing respectively. During command execution I scan the tokens for `${name}`. If I find one, I get the value from `get_shell_var()`. Before the shell exits (or after an interrupt) I call `free_shell_vars()` to walk through the array and free every item in the global `shell_vars` array, so valgrind is happy.

NOTE:
In my implementation, variables MUST not have spaces in them. if you have `var=two words` it will give an error. However, it also doesnt support quotes so that was an oversight on my end. I'm definitely changing this for A2


## Piping
In `execFullCommandLine()` I look for each `|` token and create a new pipe for it. The write end of that pipe goes into `execSingleCommand()` as its `writePipe`, and the read end from the previous pipe goes in as `readPipe` (which does not apply to the first command). Inside the child process I `dup()` those ends onto stdin/stdout and close both originals. After the child is launched the parent closes both ends of the old pipe, then shifts the newest pipe (just used as the write end) into the second slot so the next command in the chain will read from it. This is then repeated and if there is another pipe found then it creates a new write pipe.


## Globbing
Once variables are substituted I run each token through `glob()` with `GLOB_NOCHECK`. If the glob is in the first token, then I instantly only take the first match since that is that command being run. All matches for arguments go into a growing temporary token array `expandedTokens[]`. At the end I overwrite the `tokens[]` array with the expanded list, update `nTokens`, and append a NULL so that it will get parsed and exec'd correctly.


## Signals
In order to get signals to work, I had to add a struct and some functions in `xish_state.c`. The struct contains PID and job_id for each process (foreground ones dont have job_id though). Those structs are then stored in a global array that can be dynamically allocated called `static ChildProcess *child_processes`. I then made a whole lot of helper functions to manage the children states for different purposes:
```c
int get_num_children();
void add_child_pid(pid_t pid, int isBackground);
void remove_child_pid(pid_t pid);
void cleanup_zombie_children();
void kill_children();
int get_job_id(pid_t pid);
int get_pid(int job_id);
```


## Challenges
One of the hardest parts was starting the assignment and trying to figure out where I should put what. Some of my functions in xish_run.c should be split up more than that but they aren't. Also I have two functions `execFullCommandLine` and `execSingleCommand` that take in a lot of arguments and I think I would have organized it differently if I did it again. Probably more small functions for each task given instead of just a ton of if-else blocks in two main functions

Piping was also fairly difficult to work out, but most of the work was just in thinking about it and it wasnt very hard to implement after I had a plan.