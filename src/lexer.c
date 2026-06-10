#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "lexer.h"

typedef struct {
    const char *keyword;
    TokenType type;
} KeywordMap;

static int compare_keywords(const void *a, const void *b) {
    return strcasecmp(((KeywordMap *)a)->keyword, ((KeywordMap *)b)->keyword);
}

static const KeywordMap keyword_table[] = {
    {"ABS", TOKEN_ABS},
    {"AS", TOKEN_AS},
    {"ASC", TOKEN_ASC},
    {"ATN", TOKEN_ATN},
    {"BASE", TOKEN_BASE},
    {"BEEP", TOKEN_BEEP},
    {"CHDIR", TOKEN_CHDIR},
    {"CHR$", TOKEN_CHR},
    {"CIRCLE", TOKEN_CIRCLE},
    {"CLOSE", TOKEN_CLOSE},
    {"CLS", TOKEN_CLS},
    {"COLOR", TOKEN_COLOR},
    {"COS", TOKEN_COS},
    {"DATA", TOKEN_DATA},
    {"DATE$", TOKEN_DATE},
    {"DEF", TOKEN_DEF},
    {"DELETE", TOKEN_DELETE},
    {"DIM", TOKEN_DIM},
    {"DRAW", TOKEN_DRAW},
    {"ELSE", TOKEN_ELSE},
    {"END", TOKEN_END},
    {"ENVIRON", TOKEN_ENVIRON},
    {"ENVIRON$", TOKEN_ENVIRON},
    {"EOF", TOKEN_EOF_FUNC},
    {"ERASE", TOKEN_ERASE},
    {"ERROR", TOKEN_ERR},
    {"EXP", TOKEN_EXP},
    {"FILES", TOKEN_FILES},
    {"FIX", TOKEN_FIX},
    {"FOR", TOKEN_FOR},
    {"GET", TOKEN_GET},
    {"GET$", TOKEN_GETS},
    {"GOSUB", TOKEN_GOSUB},
    {"GOTO", TOKEN_GOTO},
    {"HEX$", TOKEN_HEX},
    {"IF", TOKEN_IF},
    {"INKEY$", TOKEN_INKEY},
    {"INPUT", TOKEN_INPUT},
    {"INSTR", TOKEN_INSTR},
    {"INT", TOKEN_INT},
    {"KEY", TOKEN_KEY},
    {"KILL", TOKEN_KILL},
    {"LCASE$", TOKEN_LCASE},
    {"LEFT$", TOKEN_LEFT},
    {"LEN", TOKEN_LEN},
    {"LET", TOKEN_LET},
    {"LINE", TOKEN_LINE},
    {"LIST", TOKEN_LIST},
    {"LOC", TOKEN_LOC},
    {"LOCATE", TOKEN_LOCATE},
    {"LOF", TOKEN_LOF},
    {"LOG", TOKEN_LOG},
    {"LTRIM$", TOKEN_LTRIM},
    {"MID$", TOKEN_MID},
    {"MKDIR", TOKEN_MKDIR},
    {"MOD", TOKEN_MOD},
    {"NAME", TOKEN_NAME},
    {"NEW", TOKEN_NEW},
    {"NEXT", TOKEN_NEXT},
    {"OCT$", TOKEN_OCT},
    {"ON", TOKEN_ON},
    {"OPEN", TOKEN_OPEN},
    {"OPTION", TOKEN_OPTION},
    {"PAINT", TOKEN_PAINT},
    {"PEEK", TOKEN_PEEK},
    {"PLAY", TOKEN_PLAY},
    {"POKE", TOKEN_POKE},
    {"PRINT", TOKEN_PRINT},
    {"PSET", TOKEN_PSET},
    {"PUT", TOKEN_PUT},
    {"QUIT", TOKEN_QUIT},
    {"RANDOMIZE", TOKEN_RANDOMIZE},
    {"READ", TOKEN_READ},
    {"REM", TOKEN_REM},
    {"RESTORE", TOKEN_RESTORE},
    {"RESUME", TOKEN_RESUME},
    {"RETURN", TOKEN_RETURN},
    {"REVERSE", TOKEN_REVERSE},
    {"RIGHT$", TOKEN_RIGHT},
    {"RMDIR", TOKEN_RMDIR},
    {"RND", TOKEN_RND},
    {"RTRIM$", TOKEN_RTRIM},
    {"RUN", TOKEN_RUN},
    {"SCREEN", TOKEN_SCREEN},
    {"SEEK", TOKEN_SEEK},
    {"SGN", TOKEN_SGN},
    {"SHELL", TOKEN_SHELL},
    {"SIN", TOKEN_SIN},
    {"SLEEP", TOKEN_SLEEP},
    {"SOUND", TOKEN_SOUND},
    {"SPACE$", TOKEN_SPACE},
    {"SPC", TOKEN_SPC},
    {"SQR", TOKEN_SQR},
    {"STEP", TOKEN_STEP},
    {"STR$", TOKEN_STR},
    {"STRING$", TOKEN_STRING_FUNC},
    {"SWAP", TOKEN_SWAP},
    {"SYSTEM", TOKEN_SYSTEM},
    {"TAB", TOKEN_TAB},
    {"TAN", TOKEN_TAN},
    {"THEN", TOKEN_THEN},
    {"TIME$", TOKEN_TIME},
    {"TIMER", TOKEN_TIMER},
    {"TO", TOKEN_TO},
    {"TRIM$", TOKEN_TRIM},
    {"UCASE$", TOKEN_UCASE},
    {"USING", TOKEN_USING},
    {"VAL", TOKEN_VAL},
    {"VARPTR", TOKEN_VARPTR},
    {"WEND", TOKEN_WEND},
    {"WHILE", TOKEN_WHILE},
};


Token get_next_token(const char **input) {
    Token token;
    token.text[0] = '\0';
    
    while (isspace(**input)) (*input)++;

    if (**input == '\0') {
        token.type = TOKEN_EOF;
        return token;
    }

    if (isdigit(**input) || **input == '.') {
        char buffer[32];
        int i = 0;
        while (isdigit(**input) || **input == '.') buffer[i++] = *(*input)++;
        buffer[i] = '\0';
        token.type = TOKEN_NUMBER;
        token.double_val = atof(buffer);
        token.int_val = (int)token.double_val;
        return token;
    }

    if (**input == '"') {
        (*input)++;
        int i = 0;
        while (**input != '"' && **input != '\0') token.text[i++] = *(*input)++;
        if (**input == '"') (*input)++;
        token.text[i] = '\0';
        token.type = TOKEN_STRING;
        return token;
    }

    if (**input == '\'') {
        // Skip to end of line for BASIC single-quote comments
        while (**input != '\0' && **input != '\n') (*input)++;
        token.type = TOKEN_APOSTROPHE; // Treat apostrophe as a comment token
        return token;
    }

    if (**input == '=') {
        (*input)++;
        token.type = TOKEN_EQUALS;
        return token;
    }
    if (**input == ',') { (*input)++; token.type = TOKEN_COMMA; return token; }
    if (**input == ';') { (*input)++; token.type = TOKEN_SEMICOLON; return token; }
    if (**input == ':') { (*input)++; token.type = TOKEN_COLON; return token; }
    if (**input == '<') { (*input)++; token.type = TOKEN_LESS; return token; }
    if (**input == '>') { (*input)++; token.type = TOKEN_GREATER; return token; }

    if (**input == '+') { (*input)++; token.type = TOKEN_PLUS; return token; }
    if (**input == '-') { (*input)++; token.type = TOKEN_MINUS; return token; }
    if (**input == '*') { (*input)++; token.type = TOKEN_STAR; return token; }
    if (**input == '/') { (*input)++; token.type = TOKEN_SLASH; return token; }
    if (**input == '^') { (*input)++; token.type = TOKEN_POWER; return token; }
    if (**input == '(') { (*input)++; token.type = TOKEN_LPAREN; return token; }
    if (**input == ')') { (*input)++; token.type = TOKEN_RPAREN; return token; }
    if (**input == '\\') { (*input)++; token.type = TOKEN_IDIV; return token; }
    if (**input == '#') { (*input)++; token.type = TOKEN_HASH; strcpy(token.text, "#"); return token; }

    char buffer[64];
    int i = 0;
    // Accept alphanumeric, type suffixes ($, %, !, #), and underscore in identifiers
    while ((isalnum(**input) || **input == '$' || **input == '%' || **input == '!' || **input == '#' || **input == '_') && i < 63) buffer[i++] = *(*input)++;
    buffer[i] = '\0';

    KeywordMap key = {buffer, 0};
    KeywordMap *res = bsearch(&key, keyword_table, sizeof(keyword_table) / sizeof(keyword_table[0]), sizeof(KeywordMap), compare_keywords);

    if (res) {
        token.type = res->type;
    } else {
        token.type = TOKEN_IDENTIFIER;
        strcpy(token.text, buffer);
    }

    if (i == 0 && **input != '\0') {
        token.type = TOKEN_ERROR;
        token.text[0] = *(*input)++;
        token.text[1] = '\0';
    }
    return token;
}
