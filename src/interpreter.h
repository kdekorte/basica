#ifndef INTERPRETER_H
#define INTERPRETER_H
#include <signal.h>

void interpret_line(const char *input, int is_direct);
void run_program();
void basic_output(const char *text); // Expose for program.c
void basica_trigger_key_event(int key_idx);
extern volatile sig_atomic_t stop_running;

#endif
