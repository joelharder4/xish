#ifndef __SQUISH_STATE_HEADER__
#define __SQUISH_STATE_HEADER__

int get_num_children();
void add_child_pid(pid_t pid, int isBackground);
void remove_child_pid(pid_t pid);
void cleanup_zombie_children();
void kill_children();
int get_job_id(pid_t pid);
int get_pid(int job_id);

void add_shell_var(char* name, char* value);
void get_shell_var(char* name, char** value);
void free_shell_vars();

#endif /* __SQUISH_STATE_HEADER__ */
