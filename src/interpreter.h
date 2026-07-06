#ifndef INTERPRETER_H
#define INTERPRETER_H
#include <signal.h>

void interpret_line(const char *input, int is_direct, int *last_line_num);
void run_program();
void basic_output(const char *text); // Expose for program.c
void basica_trigger_key_event(int key_idx);
void basica_trigger_strig_event(int strig_idx, int pressed);
extern volatile sig_atomic_t stop_running;

#endif
