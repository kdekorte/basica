#ifndef PROGRAM_H
#define PROGRAM_H
#include "common.h"

void add_line(int line_num, const char *text);
void list_program();
void clear_program();
Statement* get_head();
Statement* find_line(int line_num);
Statement* find_next_statement(int line_num);
void delete_line(int line_num);

#endif
