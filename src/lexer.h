#ifndef LEXER_H
#define LEXER_H
#include "common.h"

Token get_next_token(const char **input);
int find_variable(const char *name); // Declare find_variable for lexer.c
void tokenize_line(Statement *stmt);

#endif
