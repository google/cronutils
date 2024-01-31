#ifndef __CRONUTILS_SUBPROCESS_H
#define __CRONUTILS_SUBPROCESS_H

void kill_process_group(void);
int run_subprocess(char* command, char** args, void (*pre_wait_function)(void));

#endif /* __CRONUTILS_SUBPROCESS_H */
