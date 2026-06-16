#ifndef LEXER_H
#define LEXER_H
#include "common.h"

Token get_next_token(const char **input);
void tokenize_line(Statement *stmt);

#endif
