#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <glob.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/select.h>
#include "interpreter.h"
#include "lexer.h"
#include "program.h"
#include "graphics.h"
#include "audio.h"

volatile sig_atomic_t stop_running = 0;

static Variable vars[1024]; 
static int var_count = 0;
static int print_col = 0;
static int internal_argc = 0;
static char **internal_argv = NULL;
static char internal_command_line[1024] = "";

static unsigned char basica_memory[65536];

static char default_type_map[26] = {
    '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!',
    '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!'
};

static FILE *file_handles[16] = {NULL};
static unsigned int rnd_seed = 1;
static double last_rnd_value = 0.0;
static int on_error_goto_line = 0;
static int runtime_error_occurred = 0;
static int last_runtime_error_code = 0;
static int last_runtime_error_line = 0;
static int current_executing_line = 0;
static char last_runtime_error_msg[256] = "";
static Statement *error_stmt = NULL;
static const char *error_ptr = NULL;
static const char *error_next_ptr = NULL;

typedef struct {
    char name[32];
    char param_name[32];
    char expression[256];
} UserFunction;

static UserFunction user_functions[64];
static int user_function_count = 0;

void set_args(int argc, char **argv) {
    internal_argc = argc;
    internal_argv = argv;
    internal_command_line[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1) strncat(internal_command_line, " ", sizeof(internal_command_line) - 1);
        strncat(internal_command_line, argv[i], sizeof(internal_command_line) - 1);
    }
}

static const char* get_error_message(RuntimeError code) {
    switch (code) {
        case ERR_NEXT_WITHOUT_FOR: return "NEXT without FOR";
        case ERR_SYNTAX_ERROR: return "Syntax error";
        case ERR_RETURN_WITHOUT_GOSUB: return "RETURN without GOSUB";
        case ERR_OUT_OF_DATA: return "Out of DATA";
        case ERR_ILLEGAL_FUNCTION_CALL: return "Illegal function call";
        case ERR_OVERFLOW: return "Overflow";
        case ERR_OUT_OF_MEMORY: return "Out of memory";
        case ERR_UNDEFINED_LINE_NUMBER: return "Undefined line number";
        case ERR_SUBSCRIPT_OUT_OF_RANGE: return "Subscript out of range";
        case ERR_DUPLICATE_DEFINITION: return "Duplicate Definition";
        case ERR_DIVISION_BY_ZERO: return "Division by zero";
        case ERR_TYPE_MISMATCH: return "Type mismatch";
        case ERR_OUT_OF_STRING_SPACE: return "Out of string space";
        case ERR_CANT_RESUME: return "Can't RESUME";
        case ERR_RESUME_WITHOUT_ERROR: return "RESUME without error";
        case ERR_FOR_WITHOUT_NEXT: return "FOR without NEXT";
        case ERR_WHILE_WITHOUT_WEND: return "WHILE without WEND";
        case ERR_WEND_WITHOUT_WHILE: return "WEND without WHILE";
        case ERR_FIELD_OVERFLOW: return "FIELD overflow";
        case ERR_INTERNAL_ERROR: return "Internal error";
        case ERR_BAD_FILE_NUMBER: return "Bad file number";
        case ERR_FILE_NOT_FOUND: return "File not found";
        case ERR_BAD_FILE_MODE: return "Bad file mode";
        case ERR_FILE_ALREADY_OPEN: return "File already open";
        case ERR_DEVICE_IO_ERROR: return "Device I/O error";
        case ERR_FILE_ALREADY_EXISTS: return "File already exists";
        case ERR_BAD_RECORD_LENGTH: return "Bad record length";
        case ERR_DISK_FULL: return "Disk full";
        case ERR_INPUT_PAST_END: return "Input past end";
        case ERR_BAD_RECORD_NUMBER: return "Bad record number";
        case ERR_BAD_FILE_NAME: return "Bad file name";
        case ERR_TOO_MANY_FILES: return "Too many files";
        case ERR_PERMISSION_DENIED: return "Permission Denied";
        case ERR_DISK_NOT_READY: return "Disk not ready";
        case ERR_PATH_FILE_ACCESS_ERROR: return "Path/File access error";
        case ERR_PATH_NOT_FOUND: return "Path not found";
        default: return "Unprintable error";
    }
}

static void report_runtime_error(RuntimeError code) {
    const char *msg = get_error_message(code);
    last_runtime_error_code = (int)code;
    last_runtime_error_line = current_executing_line;
    
    if (msg) {
        strncpy(last_runtime_error_msg, msg, sizeof(last_runtime_error_msg) - 1);
        last_runtime_error_msg[sizeof(last_runtime_error_msg) - 1] = '\0';
    } else {
        last_runtime_error_msg[0] = '\0';
    }
    
    if (on_error_goto_line == 0) {
        char full_msg[300];
        if (msg) {
            snprintf(full_msg, sizeof(full_msg), "%s in %d\n", msg, current_executing_line);
            basic_output(full_msg);
        }
    }
    runtime_error_occurred = 1;
    stop_running = 1;
}

static double basica_rnd_next(void) {
    // Standard 31-bit LCG for cross-platform consistency.
    // This ensures RND results are the same on Windows, macOS, and Linux.
    rnd_seed = (rnd_seed * 1103515245 + 12345) & 0x7fffffff;
    last_rnd_value = (double)rnd_seed / 2147483648.0;
    return last_rnd_value;
}
static int option_base = 0;
static int option_base_set = 0;
static int arrays_dimensioned = 0;

static Statement *data_stmt = NULL;
static const char *data_ptr = NULL;
static int parse_string_expression(const char **input, char *out, int out_size);
static int find_variable(const char *name);
static int is_string_var(const char *name);
static int parse_array_index(const char **input, int var_idx);
static void set_string_variable(int idx, int array_idx, const char *value);
double evaluate_expression(const char **input);

static void clear_data_pointer(void) {
    data_stmt = NULL;
    data_ptr = NULL;
}

static int is_data_statement(Statement *stmt) {
    if (!stmt) return 0;
    const char *p = stmt->raw_command;
    Token t = get_next_token(&p);
    return t.type == TOKEN_DATA;
}

static Statement *find_next_data_statement(Statement *start) {
    Statement *s = start;
    while (s) {
        if (is_data_statement(s)) return s;
        s = s->next;
    }
    return NULL;
}

static void reset_data_pointer(void) {
    Statement *head = get_head();
    data_stmt = find_next_data_statement(head);
    if (data_stmt) {
        data_ptr = data_stmt->raw_command;
        get_next_token(&data_ptr);
    } else {
        data_ptr = NULL;
    }
}

static int advance_data_statement(void) {
    if (!data_stmt) return 0;
    data_stmt = find_next_data_statement(data_stmt->next);
    if (!data_stmt) {
        data_ptr = NULL;
        return 0;
    }
    data_ptr = data_stmt->raw_command;
    get_next_token(&data_ptr);
    return 1;
}

static int get_stdin_char(void) {
    fd_set fds;
    struct timeval tv = {0, 0};
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int ready = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (ready <= 0) return 0;
    int c = getchar();
    if (c == EOF || c == '\r' || c == '\n') return 0;
    return c;
}

static void restore_data_to_line(int line) {
    if (line <= 0) {
        reset_data_pointer();
        return;
    }
    Statement *stmt = find_line(line);
    if (!stmt) {
        reset_data_pointer();
        return;
    }
    data_stmt = stmt;
    data_ptr = data_stmt->raw_command;
    Token t = get_next_token(&data_ptr);
    if (t.type != TOKEN_DATA) {
        if (!advance_data_statement()) {
            clear_data_pointer();
        }
    }
}

static int get_next_data_item(char *out, int out_size) {
    while (1) {
        if (!data_stmt) reset_data_pointer();
        if (!data_stmt) return 0;
        while (isspace((unsigned char)*data_ptr)) data_ptr++;
        if (*data_ptr == '\0') {
            if (!advance_data_statement()) return 0;
            continue;
        }
        parse_string_expression(&data_ptr, out, out_size);
        while (isspace((unsigned char)*data_ptr)) data_ptr++;
        if (*data_ptr == ',') {
            data_ptr++;
        } else if (*data_ptr == ':') {
            data_ptr++;
            advance_data_statement();
        }
        return 1;
    }
}

static int parse_read_variable(const char **input, int *var_idx, int *array_idx, int *is_string) {
    Token var = get_next_token(input);
    if (var.type != TOKEN_IDENTIFIER) return 0;
    *is_string = is_string_var(var.text);
    *var_idx = find_variable(var.text);
    *array_idx = parse_array_index(input, *var_idx);
    return 1;
}

static int read_next_data_into_variable(const char **input) {
    int var_idx, array_idx, is_string;
    if (!parse_read_variable(input, &var_idx, &array_idx, &is_string)) return 0;
    char item[256] = "";
    if (!get_next_data_item(item, sizeof(item))) {
        report_runtime_error(ERR_OUT_OF_DATA);
        return 0;
    }
    if (is_string) {
        set_string_variable(var_idx, array_idx, item);
    } else {
        double val = atof(item);

        // Round to nearest integer if the variable has the '%' suffix
        const char *name = vars[var_idx].name;
        size_t len = strlen(name);
        if (len > 0 && name[len - 1] == '%') {
            val = (double)(short)round(val);
        }

        if (array_idx >= 0 && vars[var_idx].array && array_idx >= 0 && array_idx < vars[var_idx].array_size) {
            vars[var_idx].array[array_idx] = val;
        } else {
            vars[var_idx].value = val;
        }
    }
    return 1;
}

static void randomize_seed(const char **input) {
    const char *saved = *input;
    Token next = get_next_token(input);
    unsigned seed;
    if (next.type == TOKEN_TIMER) {
        seed = (unsigned)clock();
    } else if (next.type == TOKEN_NUMBER) {
        seed = (unsigned)next.int_val;
    } else if (next.type == TOKEN_EOF || next.type == TOKEN_COLON) {
        *input = saved;
        seed = (unsigned)time(NULL) ^ (unsigned)clock();
    } else {
        *input = saved;
        seed = (unsigned)evaluate_expression(input);
    }
    rnd_seed = seed;
}

static void parse_def_range(const char **ptr, char type_char) {
    while (1) {
        Token t = get_next_token(ptr);
        if (t.type != TOKEN_IDENTIFIER) break;
        
        char start = toupper((unsigned char)t.text[0]);
        char end = start;
        
        const char *saved = *ptr;
        Token dash = get_next_token(ptr);
        if (dash.type == TOKEN_MINUS) {
            Token t2 = get_next_token(ptr);
            if (t2.type == TOKEN_IDENTIFIER) {
                end = toupper((unsigned char)t2.text[0]);
            } else {
                *ptr = saved;
            }
        } else {
            *ptr = saved;
        }
        
        for (char c = start; c <= end; c++) {
            if (c >= 'A' && c <= 'Z') default_type_map[c - 'A'] = type_char;
        }
        
        saved = *ptr;
        if (get_next_token(ptr).type != TOKEN_COMMA) {
            *ptr = saved;
            break;
        }
    }
}

static int parse_read_list(const char **input) {
    if (!read_next_data_into_variable(input)) return 0;
    while (1) {
        const char *saved = *input;
        Token sep = get_next_token(input);
        if (sep.type == TOKEN_COMMA) {
            if (!read_next_data_into_variable(input)) return 0;
            continue;
        }
        *input = saved;
        break;
    }
    return 1;
}

static int parse_restore(const char **input) {
    const char *saved = *input;
    Token arg = get_next_token(input);
    if (arg.type == TOKEN_NUMBER) {
        restore_data_to_line(arg.int_val);
        return 1;
    }
    *input = saved;
    reset_data_pointer();
    return 1;
}

static int parse_randomize(const char **input) {
    randomize_seed(input);
    return 1;
}

static int is_string_var(const char *name) {
    int len = (int)strlen(name);
    if (len == 0) return 0;
    char last = name[len - 1];
    if (last == '$') return 1;
    if (last == '%' || last == '!' || last == '#') return 0;
    int first = toupper((unsigned char)name[0]);
    if (first >= 'A' && first <= 'Z') {
        return (default_type_map[first - 'A'] == '$');
    }
    return 0;
}

static int find_variable(const char *name) {
    char normalized[64];
    strncpy(normalized, name, 63);
    normalized[63] = '\0';
    int len = (int)strlen(normalized);
    if (len > 0) {
        char last = normalized[len - 1];
        if (last != '$' && last != '%' && last != '!' && last != '#') {
            int first = toupper((unsigned char)normalized[0]);
            if (first >= 'A' && first <= 'Z') {
                char suffix = default_type_map[first - 'A'];
                if (suffix != '\0' && suffix != '!') {
                    normalized[len] = suffix;
                    normalized[len+1] = '\0';
                }
            }
        }
    }
    for (int i = 0; i < var_count; i++) {
        if (strcasecmp(vars[i].name, normalized) == 0) {
            return i;
        }
    }
    if (var_count < 1024) {
        memset(&vars[var_count], 0, sizeof(Variable));
        strncpy(vars[var_count].name, normalized, 31);
        vars[var_count].name[31] = '\0';
        return var_count++;
    }
    return -1;
}

typedef struct {
    Statement *stmt;
    const char *ptr;
} Subroutine;

static Subroutine gosub_call_stack[32];
static int gosub_ptr = 0;

typedef struct {
    int var_idx;
    double end_val;
    double step_val;
    Statement *start_stmt;
    const char *start_ptr;
} ForLoop;

static ForLoop for_stack[16];
static int for_ptr = 0;

typedef struct {
    Statement *stmt;
    const char *ptr;
} WhileLoop;
static WhileLoop while_stack[16];
static int while_ptr = 0;

// Helper function to convert multi-dimensional indices to linear index
static int calc_linear_index(Variable *var, int *indices, int num_indices) {
    if (var->num_dims == 0 || var->num_dims != num_indices) {
        report_runtime_error(ERR_SUBSCRIPT_OUT_OF_RANGE);
        return -1;
    }
    
    int linear = 0;
    int multiplier = 1;
    for (int i = var->num_dims - 1; i >= 0; i--) {
        int max_subscript = var->dims[i];
        int val = indices[i];
        if (val < option_base || val > max_subscript) {
            report_runtime_error(ERR_SUBSCRIPT_OUT_OF_RANGE);
            return -1;
        }
        int offset = (option_base == 0) ? val : (val - 1);
        linear += offset * multiplier;
        
        int dim_size = (option_base == 0) ? (max_subscript + 1) : max_subscript;
        multiplier *= dim_size;
    }
    return linear;
}

static void ensure_array_dimensioned(int idx, int num_dims) {
    if (vars[idx].array || vars[idx].s_array) return; // already dimensioned
    
    // Auto-dimension to 10 for each dimension
    int total_size = 1;
    for (int i = 0; i < num_dims; i++) {
        int dim_size = (option_base == 0) ? 11 : 10;
        total_size *= dim_size;
    }
    
    if (vars[idx].name[strlen(vars[idx].name)-1] == '$') {
        vars[idx].s_array = calloc(total_size, sizeof(char*));
    } else {
        vars[idx].array = malloc(total_size * sizeof(double));
        for (int i = 0; i < total_size; i++) vars[idx].array[i] = 0;
    }
    vars[idx].array_size = total_size;
    vars[idx].num_dims = num_dims;
    for (int i = 0; i < num_dims; i++) vars[idx].dims[i] = 10;
    
    arrays_dimensioned = 1;
}

static int parse_array_index(const char **input, int var_idx) {
    const char *saved = *input;
    Token t = get_next_token(input);
    if (t.type != TOKEN_LPAREN) {
        *input = saved;
        return -1;
    }

    int indices[3] = {0};
    int num_indices = 0;
    indices[0] = (int)evaluate_expression(input);
    num_indices = 1;

    while (num_indices < 3) {
        const char *sep_saved = *input;
        Token sep = get_next_token(input);
        if (sep.type != TOKEN_COMMA) {
            *input = sep_saved;
            break;
        }
        indices[num_indices++] = (int)evaluate_expression(input);
    }

    Token close = get_next_token(input);
    if (close.type != TOKEN_RPAREN) {
        *input = saved;
        return -1;
    }

    // Auto-dimension if not already dimensioned
    ensure_array_dimensioned(var_idx, num_indices);

    return calc_linear_index(&vars[var_idx], indices, num_indices);
}

static void get_string_variable_value(int idx, int array_idx, char *dest, int dest_size) {
    const char *src = "";
    if (array_idx >= 0) {
        if (vars[idx].s_array && array_idx >= 0 && array_idx < vars[idx].array_size && vars[idx].s_array[array_idx]) {
            src = vars[idx].s_array[array_idx];
        }
    } else {
        if (vars[idx].s_value) src = vars[idx].s_value;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static void append_string_value(char *dest, const char *src, int dest_size) {
    size_t used = strlen(dest);
    if (used + 1 >= (size_t)dest_size) return;
    strncat(dest, src, dest_size - 1 - used);
}

static int parse_string_expression(const char **input, char *out, int out_size) {
    out[0] = '\0';
    int first = 1;

    while (1) {
        char term[256] = "";
        const char *saved = *input;
        Token t = get_next_token(input);

        if (t.type == TOKEN_CHR || t.type == TOKEN_TAB) {
            get_next_token(input); // (
            double arg = evaluate_expression(input);
            get_next_token(input); // )
            if (t.type == TOKEN_CHR) {
                term[0] = (unsigned char)((int)arg & 0xFF);
                term[1] = '\0';
            } else {
                int w = (int)arg; if (w < 0) w = 0; if (w >= 255) w = 255;
                memset(term, ' ', w); term[w] = '\0';
            }
        } else if (t.type == TOKEN_LEFT || t.type == TOKEN_RIGHT || t.type == TOKEN_MID) {
            TokenType ft = t.type;
            get_next_token(input); // (
            char base[256] = "";
            parse_string_expression(input, base, sizeof(base));
            get_next_token(input); // ,
            int n1 = (int)evaluate_expression(input);
            int n2 = -1;
            if (ft == TOKEN_MID) {
                const char *comma_saved = *input;
                if (get_next_token(input).type == TOKEN_COMMA) n2 = (int)evaluate_expression(input);
                else *input = comma_saved;
            }
            get_next_token(input); // )
            int slen = (int)strlen(base);
            if (ft == TOKEN_LEFT) {
                int cnt = (n1 < 0) ? 0 : (n1 > slen ? slen : n1);
                strncpy(term, base, cnt); term[cnt] = '\0';
            } else if (ft == TOKEN_RIGHT) {
                int cnt = (n1 < 0) ? 0 : (n1 > slen ? slen : n1);
                strncpy(term, base + (slen - cnt > 0 ? slen - cnt : 0), cnt); term[cnt] = '\0';
            } else { // MID
                int start = n1 - 1; if (start < 0) start = 0;
                if (start >= slen) term[0] = '\0';
                else {
                    int cnt = (n2 == -1) ? (slen - start) : n2;
                    if (cnt < 0) cnt = 0; if (start + cnt > slen) cnt = slen - start;
                    strncpy(term, base + start, cnt); term[cnt] = '\0';
                }
            }
        } else if (t.type == TOKEN_SPACE || t.type == TOKEN_SPC) {
            get_next_token(input); // (
            int n = (int)evaluate_expression(input);
            get_next_token(input); // )
            if (n < 0) n = 0; if (n > 255) n = 255;
            memset(term, ' ', n); term[n] = '\0';
        } else if (t.type == TOKEN_STRING_FUNC) {
            get_next_token(input); // (
            int n = (int)evaluate_expression(input);
            get_next_token(input); // ,
            char c = ' ';
            char arg_buf[256] = "";
            const char *saved_arg = *input;
            if (parse_string_expression(input, arg_buf, sizeof(arg_buf))) {
                if (arg_buf[0]) c = arg_buf[0];
            } else {
                *input = saved_arg;
                c = (char)evaluate_expression(input);
            }
            get_next_token(input); // )
            if (n < 0) n = 0; if (n > 255) n = 255;
            memset(term, c, n); term[n] = '\0';
        } else if (t.type == TOKEN_COMMANDS) {
            strncpy(term, internal_command_line, sizeof(term) - 1);
            term[sizeof(term) - 1] = '\0';
        } else if (t.type == TOKEN_ARGVS) {
            get_next_token(input); // (
            int idx = (int)evaluate_expression(input);
            get_next_token(input); // )
            if (idx >= 0 && idx < internal_argc && internal_argv) {
                strncpy(term, internal_argv[idx], sizeof(term) - 1);
                term[sizeof(term) - 1] = '\0';
            }
        } else if (t.type == TOKEN_GETS) {
            get_next_token(input); // (
            int fnum = -1;
            const char *saved_num = *input;
            Token hash_tok = get_next_token(input);
            if (hash_tok.type == TOKEN_HASH) {
                fnum = (int)evaluate_expression(input);
            } else {
                *input = saved_num;
                fnum = (int)evaluate_expression(input);
            }
            get_next_token(input); // ,
            int rec = (int)evaluate_expression(input);
            get_next_token(input); // ,
            int len = (int)evaluate_expression(input);
            get_next_token(input); // )
            if (fnum < 1 || fnum >= 16 || !file_handles[fnum]) {
                report_runtime_error(ERR_BAD_FILE_NUMBER);
                return 0;
            }
            long offset = (long)((rec - 1) * len);
            fseek(file_handles[fnum], offset, SEEK_SET);
            char buf[512];
            size_t r = fread(buf, 1, len, file_handles[fnum]);
            if (r < (size_t)len) {
                for (size_t i = r; i < (size_t)len; i++) buf[i] = ' ';
            }
            buf[len] = '\0';
            int trim = len - 1;
            while (trim >= 0 && buf[trim] == ' ') { buf[trim] = '\0'; trim--; }
            strncpy(term, buf, sizeof(term) - 1);
            term[sizeof(term) - 1] = '\0';
        } else if (t.type == TOKEN_INKEY) {
            if (graphics_is_active()) {
                update_graphics();
                int c = get_graphics_char();
                if (c) {
                    term[0] = (char)c;
                    term[1] = '\0';
                }
            } else {
                int c = get_stdin_char();
                if (c) {
                    term[0] = (char)c;
                    term[1] = '\0';
                }
            }
        } else if (t.type == TOKEN_UCASE || t.type == TOKEN_LCASE || t.type == TOKEN_TRIM || t.type == TOKEN_LTRIM || t.type == TOKEN_RTRIM) {
            TokenType ft = t.type;
            get_next_token(input); // (
            char base[256] = "";
            parse_string_expression(input, base, sizeof(base));
            get_next_token(input); // )
            if (ft == TOKEN_UCASE) {
                for (int i = 0; base[i]; i++) base[i] = toupper((unsigned char)base[i]);
            } else if (ft == TOKEN_LCASE) {
                for (int i = 0; base[i]; i++) base[i] = tolower((unsigned char)base[i]);
            } else if (ft == TOKEN_TRIM) {
                int start = 0, end = (int)strlen(base) - 1;
                while (start <= end && isspace((unsigned char)base[start])) start++;
                while (end >= start && isspace((unsigned char)base[end])) end--;
                int len = (end >= start) ? (end - start + 1) : 0;
                memmove(base, base + start, len);
                base[len] = '\0';
            } else if (ft == TOKEN_LTRIM) {
                int start = 0;
                while (base[start] && isspace((unsigned char)base[start])) start++;
                if (start > 0) {
                    int len = (int)strlen(base + start);
                    memmove(base, base + start, len + 1);
                }
            } else if (ft == TOKEN_RTRIM) {
                int end = (int)strlen(base) - 1;
                while (end >= 0 && isspace((unsigned char)base[end])) end--;
                base[end + 1] = '\0';
            }
            strncpy(term, base, sizeof(term) - 1);
            term[sizeof(term) - 1] = '\0';
        } else if (t.type == TOKEN_HEX || t.type == TOKEN_OCT) {
            get_next_token(input); // (
            double val = evaluate_expression(input);
            get_next_token(input); // )
            int intval = (int)val;
            if (t.type == TOKEN_HEX) snprintf(term, sizeof(term), "%X", intval);
            else snprintf(term, sizeof(term), "%o", intval);
        } else if (t.type == TOKEN_TIME || t.type == TOKEN_DATE) {
            time_t rawtime;
            struct tm *timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            if (t.type == TOKEN_TIME) {
                strftime(term, sizeof(term), "%H:%M:%S", timeinfo);
            } else {
                strftime(term, sizeof(term), "%m-%d-%Y", timeinfo);
            }
        } else if (t.type == TOKEN_ENVIRON) {
            get_next_token(input); // (
            char arg_val[256] = "";
            int is_str = parse_string_expression(input, arg_val, sizeof(arg_val));
            if (is_str) {
                char *ev = getenv(arg_val);
                if (ev) strncpy(term, ev, sizeof(term)-1);
            } else {
                int idx = atoi(arg_val);
                extern char **environ;
                if (environ && idx > 0) {
                    int count = 1;
                    for (char **e = environ; *e; e++, count++) {
                        if (count == idx) {
                            strncpy(term, *e, sizeof(term)-1);
                            break;
                        }
                    }
                }
            }
            get_next_token(input); // )
        } else if (t.type == TOKEN_STR) {
            get_next_token(input); // (
            double val = evaluate_expression(input);
            get_next_token(input); // )
            if (val >= 0) snprintf(term, sizeof(term), " %g", val);
            else snprintf(term, sizeof(term), "%g", val);
        } else if (t.type == TOKEN_STRING) {
            strncpy(term, t.text, sizeof(term) - 1);
        } else if (t.type == TOKEN_IDENTIFIER && is_string_var(t.text)) {
            int idx = find_variable(t.text);
            int array_idx = parse_array_index(input, idx);
            char temp[256] = "";
            get_string_variable_value(idx, array_idx, temp, sizeof(temp));
            strncpy(term, temp, sizeof(term) - 1);
        } else {
            if (first) {
                *input = saved;
                double val = evaluate_expression(input);
                snprintf(out, out_size, "%g", val);
                return 0;
            }
            *input = saved;
            break;
        }

        append_string_value(out, term, out_size);
        first = 0;

        const char *sep_saved = *input;
        if (get_next_token(input).type != TOKEN_PLUS) {
            *input = sep_saved;
            break;
        }
    }
    return 1;
}

static void set_string_variable(int idx, int array_idx, const char *value) {
    if (array_idx >= 0) {
        if (!vars[idx].s_array || array_idx < 0 || array_idx >= vars[idx].array_size) return;
        if (vars[idx].s_array[array_idx]) free(vars[idx].s_array[array_idx]);
        vars[idx].s_array[array_idx] = strdup(value);
    } else {
        if (vars[idx].s_value) free(vars[idx].s_value);
        vars[idx].s_value = strdup(value);
    }
}

static void trim_string(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) *end-- = '\0';
}

static void assign_input_value(int idx, int array_idx, int is_string, const char *value) {
    if (is_string) {
        if (array_idx >= 0 && vars[idx].s_array && array_idx < vars[idx].array_size) {
            if (vars[idx].s_array[array_idx]) free(vars[idx].s_array[array_idx]);
            vars[idx].s_array[array_idx] = strdup(value);
        } else {
            if (vars[idx].s_value) free(vars[idx].s_value);
            vars[idx].s_value = strdup(value);
        }
    } else {
        double val = atof(value);
        // Round to nearest integer if the variable has the '%' suffix
        const char *name = vars[idx].name;
        size_t len = strlen(name);
        if (len > 0 && name[len - 1] == '%') {
            val = (double)(short)round(val);
        }
        if (array_idx >= 0 && vars[idx].array && array_idx < vars[idx].array_size) {
            vars[idx].array[array_idx] = val;
        } else {
            vars[idx].value = val;
        }
    }
}

static void apply_basica_using(const char *fmt, double val, char *out, int out_size) {
    if (strchr(fmt, '%')) {
        snprintf(out, out_size, fmt, val);
        return;
    }
    int hashes_before = 0, hashes_after = 0, has_dot = 0;
    for (int i = 0; fmt[i]; i++) {
        if (fmt[i] == '#') { if (has_dot) hashes_after++; else hashes_before++; }
        else if (fmt[i] == '.') has_dot = 1;
    }
    if (has_dot) snprintf(out, out_size, "%*.*f", hashes_before + hashes_after + 1, hashes_after, val);
    else if (hashes_before > 0) snprintf(out, out_size, "%*d", hashes_before, (int)val);
    else snprintf(out, out_size, "%g", val);
}

static void apply_basica_using_str(const char *fmt, const char *val, char *out, int out_size) {
    if (strchr(fmt, '%')) {
        snprintf(out, out_size, fmt, val);
        return;
    }
    if (strcmp(fmt, "!") == 0) {
        snprintf(out, out_size, "%.1s", val);
    } else if (strcmp(fmt, "&") == 0) {
        snprintf(out, out_size, "%s", val);
    } else {
        snprintf(out, out_size, "%s", val);
    }
}

static void execute_assignment(const char **input, Token var_token) {
    int idx = find_variable(var_token.text);
    int array_idx = parse_array_index(input, idx);
    const char *saved = *input;
    Token eq = get_next_token(input);
    if (eq.type != TOKEN_EQUALS) {
        *input = saved;
        report_runtime_error(ERR_SYNTAX_ERROR);
        return;
    }

    if (is_string_var(var_token.text)) {
        char value[256] = "";
        const char *expr_saved = *input;
        Token tok = get_next_token(input);
        if (tok.type == TOKEN_STRING) {
            strncpy(value, tok.text, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
        } else {
            *input = expr_saved;
            parse_string_expression(input, value, sizeof(value));
        }
        set_string_variable(idx, array_idx, value);
    } else {
        double val = evaluate_expression(input);
        // Round to nearest integer if the variable has the '%' suffix
        const char *name = vars[idx].name;
        size_t len = strlen(name);
        if (len > 0 && name[len - 1] == '%') {
            val = (double)(short)round(val);
        }
        if (array_idx >= 0 && vars[idx].array && array_idx >= 0 && array_idx < vars[idx].array_size) {
            vars[idx].array[array_idx] = val;
        } else {
            vars[idx].value = val;
        }
    }
}

static void clear_variables(void) {
    for (int i = 0; i < var_count; i++) {
        if (vars[i].array) {
            free(vars[i].array);
            vars[i].array = NULL;
        }
        if (vars[i].s_array) {
            for (int j = 0; j < vars[i].array_size; j++) {
                free(vars[i].s_array[j]);
            }
            free(vars[i].s_array);
            vars[i].s_array = NULL;
        }
        if (vars[i].s_value) {
            free(vars[i].s_value);
            vars[i].s_value = NULL;
        }
        vars[i].array_size = 0;
        vars[i].num_dims = 0;
        vars[i].value = 0;
        vars[i].name[0] = '\0';
    }
    var_count = 0;
    user_function_count = 0;
    option_base = 0;
    option_base_set = 0;
    arrays_dimensioned = 0;
    for (int i = 0; i < 26; i++) default_type_map[i] = '!';
}

void basic_output(const char *text) { // Made non-static
    if (!graphics_is_active()) {
        printf("%s", text);
        fflush(stdout);
    }
    graphics_print(text);
    for (int i = 0; text[i]; i++) {
        if (text[i] == '\n' || text[i] == '\r') {
            print_col = 0;
        } else {
            print_col++;
        }
    }
}

// Forward declarations for recursive descent expression parser
double evaluate_expression(const char **input);

static double primary(const char **input) {
    Token t = get_next_token(input);
    if (t.type == TOKEN_NUMBER) return t.double_val;
    if (t.type == TOKEN_ARGC) return (double)internal_argc;
    if (t.type == TOKEN_IDENTIFIER) {
        if (strcasecmp(t.text, "ERR") == 0) return (double)last_runtime_error_code;
        if (strcasecmp(t.text, "ERL") == 0) return (double)last_runtime_error_line;

        if (strncasecmp(t.text, "FN", 2) == 0) {
            const char *lp_ptr = *input;
            if (get_next_token(&lp_ptr).type == TOKEN_LPAREN) {
                for (int i = 0; i < user_function_count; i++) {
                    if (strcasecmp(user_functions[i].name, t.text) == 0) {
                        *input = lp_ptr;
                        double arg_val = evaluate_expression(input);
                        get_next_token(input); // consume ')'
                        
                        int p_idx = find_variable(user_functions[i].param_name);
                        double old_val = vars[p_idx].value;
                        vars[p_idx].value = arg_val;
                        const char *expr_ptr = user_functions[i].expression;
                        double result = evaluate_expression(&expr_ptr);
                        vars[p_idx].value = old_val;
                        return result;
                    }
                }
            }
        }
        int idx = find_variable(t.text);
        const char *look_ptr = *input; // Peek to see if it's an array access
        Token next = get_next_token(&look_ptr);
        if (next.type == TOKEN_LPAREN) {
            int array_idx = parse_array_index(input, idx);
            if (vars[idx].s_array && array_idx >= 0 && array_idx < vars[idx].array_size) {
                // This is a string array, evaluate_expression currently only handles doubles.
                // We return 0 here as fallback. String expressions are handled in statements.
                return 0;
            }
            if (vars[idx].array && array_idx >= 0 && array_idx < vars[idx].array_size) {
                return vars[idx].array[array_idx];
            }
            return 0;
        }
        return (idx != -1) ? vars[idx].value : 0;
    }
    if (t.type == TOKEN_LPAREN) {
        double val = evaluate_expression(input);
        get_next_token(input); // consume ')'
        return val;
    }
    if ((t.type >= TOKEN_ABS && t.type <= TOKEN_SGN) || t.type == TOKEN_EOF_FUNC || t.type == TOKEN_TIMER || t.type == TOKEN_KEY || t.type == TOKEN_ASC || t.type == TOKEN_LEN || t.type == TOKEN_INSTR || t.type == TOKEN_VAL || t.type == TOKEN_PEEK || t.type == TOKEN_VARPTR || t.type == TOKEN_LOF || t.type == TOKEN_LOC) {
        TokenType ft = t.type;
        const char *saved = *input;
        Token next = get_next_token(input);
        double arg = 0;
        int has_arg = 0;
        if (next.type == TOKEN_LPAREN) {
            has_arg = 1;
            if (ft == TOKEN_VARPTR) {
                Token var_tok = get_next_token(input);
                int var_idx = -1;
                int array_idx = -1;
                if (var_tok.type == TOKEN_IDENTIFIER) {
                    var_idx = find_variable(var_tok.text);
                    array_idx = parse_array_index(input, var_idx);
                }
                get_next_token(input); // consume ')'
                if (var_idx < 0) return 0;
                if (array_idx >= 0) return 61440.0 + var_idx * 256.0 + array_idx;
                return 61440.0 + var_idx * 256.0;
            }
            if (ft == TOKEN_ASC) {
                char buf[256] = "";
                parse_string_expression(input, buf, sizeof(buf));
                get_next_token(input); // consume ')'
                return buf[0] ? (double)(unsigned char)buf[0] : 0;
            }
            if (ft == TOKEN_LEN) {
                char buf[256] = "";
                parse_string_expression(input, buf, sizeof(buf));
                get_next_token(input); // consume ')'
                return (double)strlen(buf);
            }
            if (ft == TOKEN_VAL) {
                char buf[256] = "";
                parse_string_expression(input, buf, sizeof(buf));
                get_next_token(input); // consume ')'
                return atof(buf);
            }
            if (ft == TOKEN_INSTR) {
                char s1[256] = "", s2[256] = "";
                double start = 1.0;
                const char *lookahead = *input;
                Token first_arg = get_next_token(&lookahead);
                
                if (first_arg.type == TOKEN_NUMBER || (first_arg.type == TOKEN_IDENTIFIER && first_arg.text[strlen(first_arg.text)-1] != '$')) {
                    start = evaluate_expression(input);
                    get_next_token(input); // ,
                }
                
                parse_string_expression(input, s1, sizeof(s1));
                get_next_token(input); // ,
                parse_string_expression(input, s2, sizeof(s2));
                get_next_token(input); // )
                
                if (start < 1) start = 1;
                if (start > (double)strlen(s1)) return 0;
                char *p = strstr(s1 + (int)start - 1, s2);
                return p ? (double)(p - s1 + 1) : 0;
            }
            arg = evaluate_expression(input);
            get_next_token(input); // consume ')'
        } else {
            *input = saved;
        }
        switch((int)ft) {
            case TOKEN_ABS: return fabs(arg);
            case TOKEN_SQR: return sqrt(arg);
            case TOKEN_SIN: return sin(arg);
            case TOKEN_COS: return cos(arg);
            case TOKEN_TAN: return tan(arg);
            case TOKEN_ATN: return atan(arg);
            case TOKEN_EXP: return exp(arg);
            case TOKEN_LOG: return log(arg);
            case TOKEN_INT: return floor(arg);
            case TOKEN_FIX: return (double)(long)arg;
            case TOKEN_SGN: return (arg > 0) - (arg < 0);
            case TOKEN_EOF_FUNC: {
                int fnum = (int)arg;
                if (fnum < 1 || fnum >= 16 || !file_handles[fnum]) {
                    report_runtime_error(ERR_BAD_FILE_NUMBER);
                    return 0;
                }
                int c = fgetc(file_handles[fnum]);
                    if (c == EOF) return 1;
                    ungetc(c, file_handles[fnum]);
                    return 0;
                }
            case TOKEN_LOF: {
                int fnum = (int)arg;
                if (fnum < 1 || fnum >= 16 || !file_handles[fnum]) {
                    report_runtime_error(ERR_BAD_FILE_NUMBER);
                    return 0;
                }
                long cur = ftell(file_handles[fnum]);
                fseek(file_handles[fnum], 0, SEEK_END);
                long size = ftell(file_handles[fnum]);
                fseek(file_handles[fnum], cur, SEEK_SET);
                return (double)size;
            }
            case TOKEN_LOC: {
                int fnum = (int)arg;
                if (fnum < 1 || fnum >= 16 || !file_handles[fnum]) {
                    report_runtime_error(ERR_BAD_FILE_NUMBER);
                    return 0;
                }
                return (double)ftell(file_handles[fnum]);
            }
            case TOKEN_TIMER: return (double)clock() / CLOCKS_PER_SEC;
            case TOKEN_KEY: return (double)get_graphics_key();
            case TOKEN_PEEK: {
                int addr = (int)arg;
                if (addr < 0 || addr >= 65536) return 0;
                return basica_memory[addr];
            }
            case TOKEN_RND:
                if (has_arg && arg < 0) {
                    // RND with negative argument seeds the generator deterministically
                    rnd_seed = (unsigned int)(-(long)arg);
                    return basica_rnd_next();
                }
                if (has_arg && arg == 0) return last_rnd_value;
                return basica_rnd_next();
            default: return 0;
        }
    }
    return 0;
}

static double power_op(const char **input) {
    double val = primary(input);
    const char *saved = *input;
    Token t = get_next_token(input);
    if (t.type == TOKEN_POWER) return pow(val, power_op(input));
    *input = saved;
    return val;
}

static double unary(const char **input) {
    const char *saved = *input;
    Token t = get_next_token(input);
    if (t.type == TOKEN_PLUS) return unary(input);
    if (t.type == TOKEN_MINUS) return -unary(input);
    *input = saved;
    return power_op(input);
}

static double term(const char **input) {
    double val = unary(input);
    while (1) {
        const char *saved = *input;
        Token t = get_next_token(input);
        if (t.type == TOKEN_STAR) val *= unary(input);
        else if (t.type == TOKEN_SLASH) { double d = unary(input); if (d != 0) val /= d; }
        else if (t.type == TOKEN_MOD) { double d = unary(input); if (d != 0) val = (long)val % (long)d; }
        else if (t.type == TOKEN_IDIV) { double d = unary(input); if (d != 0) val = (long)(val / d); }
        else { *input = saved; break; }
    }
    return val;
}

static double arithmetic_expression(const char **input) {
    double val = term(input);
    while (1) {
        const char *saved = *input;
        Token t = get_next_token(input);
        if (t.type == TOKEN_PLUS) val += term(input);
        else if (t.type == TOKEN_MINUS) val -= term(input);
        else { *input = saved; break; }
    }
    return val;
}

static int is_string_token(Token t) {
    if (t.type == TOKEN_STRING) return 1;
    if (t.type == TOKEN_IDENTIFIER && is_string_var(t.text)) return 1;
    if (t.text[0] != '\0' && t.text[strlen(t.text)-1] == '$') return 1;
    return (t.type == TOKEN_CHR || t.type == TOKEN_LEFT || t.type == TOKEN_RIGHT ||
            t.type == TOKEN_MID || t.type == TOKEN_UCASE || t.type == TOKEN_LCASE ||
            t.type == TOKEN_TRIM || t.type == TOKEN_LTRIM || t.type == TOKEN_RTRIM ||
            t.type == TOKEN_STR || t.type == TOKEN_HEX || t.type == TOKEN_OCT ||
            t.type == TOKEN_STRING_FUNC || t.type == TOKEN_INKEY || t.type == TOKEN_GETS || t.type == TOKEN_ENVIRON ||
            t.type == TOKEN_TIME || t.type == TOKEN_DATE || t.type == TOKEN_TAB ||
            t.type == TOKEN_SPACE || t.type == TOKEN_SPC ||
            t.type == TOKEN_ARGVS || t.type == TOKEN_COMMANDS);
}

static double relational_expression(const char **input) {
    double val = arithmetic_expression(input);
    while (1) {
        const char *saved = *input;
        Token t = get_next_token(input);
        if (t.type == TOKEN_EQUALS) val = (val == arithmetic_expression(input)) ? -1.0 : 0.0;
        else if (t.type == TOKEN_LESS) {
            const char *s2 = *input;
            Token t2 = get_next_token(input);
            if (t2.type == TOKEN_GREATER) val = (val != arithmetic_expression(input)) ? -1.0 : 0.0;
            else if (t2.type == TOKEN_EQUALS) val = (val <= arithmetic_expression(input)) ? -1.0 : 0.0;
            else { *input = s2; val = (val < arithmetic_expression(input)) ? -1.0 : 0.0; }
        }
        else if (t.type == TOKEN_GREATER) {
            const char *s2 = *input;
            Token t2 = get_next_token(input);
            if (t2.type == TOKEN_EQUALS) val = (val >= arithmetic_expression(input)) ? -1.0 : 0.0;
            else { *input = s2; val = (val > arithmetic_expression(input)) ? -1.0 : 0.0; }
        }
        else { *input = saved; break; }
    }
    return val;
}

static double logical_not_expression(const char **input) {
    const char *saved = *input;
    Token t = get_next_token(input);
    if (t.type == TOKEN_NOT) {
        return (double)(~(short)logical_not_expression(input));
    }
    *input = saved;
    return relational_expression(input);
}

static double bitwise_and_expression(const char **input) {
    double val = logical_not_expression(input);
    while (1) {
        const char *saved = *input;
        Token t = get_next_token(input);
        if (t.type == TOKEN_AND) val = (double)((short)val & (short)logical_not_expression(input));
        else { *input = saved; break; }
    }
    return val;
}

static double bitwise_or_expression(const char **input) {
    double val = bitwise_and_expression(input);
    while (1) {
        const char *saved = *input;
        Token t = get_next_token(input);
        if (t.type == TOKEN_OR) val = (double)((short)val | (short)bitwise_and_expression(input));
        else { *input = saved; break; }
    }
    return val;
}

double evaluate_expression(const char **input) {
    const char *peek_ptr = *input;
    Token peek_tok = get_next_token(&peek_ptr);
    if (is_string_token(peek_tok)) {
        char s1[256], s2[256];
        parse_string_expression(input, s1, sizeof(s1));
        const char *op_saved = *input;
        Token t = get_next_token(input);
        if (t.type == TOKEN_EQUALS || t.type == TOKEN_LESS || t.type == TOKEN_GREATER) {
            int op1 = t.type;
            const char *saved2 = *input;
            Token t2 = get_next_token(input);
            int op2 = -1;
            if (t2.type == TOKEN_EQUALS || t2.type == TOKEN_GREATER) op2 = t2.type;
            else *input = saved2;

            parse_string_expression(input, s2, sizeof(s2));
            int res = strcmp(s1, s2);
            if (op1 == TOKEN_EQUALS) return res == 0 ? -1.0 : 0.0;
            if (op1 == TOKEN_LESS) {
                if (op2 == TOKEN_GREATER) return res != 0 ? -1.0 : 0.0;
                if (op2 == TOKEN_EQUALS) return res <= 0 ? -1.0 : 0.0;
                return res < 0 ? -1.0 : 0.0;
            }
            if (op1 == TOKEN_GREATER) {
                if (op2 == TOKEN_EQUALS) return res >= 0 ? -1.0 : 0.0;
                return res > 0 ? -1.0 : 0.0;
            }
        }
        *input = op_saved;
        return 0;
    }

    double val = bitwise_or_expression(input);
    while (1) {
        const char *saved = *input;
        Token t = get_next_token(input);
        if (t.type == TOKEN_XOR) val = (double)((short)val ^ (short)bitwise_or_expression(input));
        else { *input = saved; break; }
    }
    return val;
}

void interpret_line_at_ptr(const char **ptr_addr, int is_direct) {
    const char *ptr = *ptr_addr;
    Token t = get_next_token(&ptr);

    if (t.type == TOKEN_EOF || t.type == TOKEN_COLON) {
        *ptr_addr = ptr;
        return;
    }

    if (t.type == TOKEN_NUMBER && is_direct) {
        add_line(t.int_val, ptr);
    } else {
        if (t.type == TOKEN_ON) {
            const char *saved = ptr;
            Token next = get_next_token(&ptr);
            if (next.type == TOKEN_ERR || (next.type == TOKEN_IDENTIFIER && strcasecmp(next.text, "ERROR") == 0)) {
                Token g = get_next_token(&ptr);
                if (g.type == TOKEN_GOTO) {
                    Token target = get_next_token(&ptr);
                    if (target.type == TOKEN_NUMBER) {
                        on_error_goto_line = target.int_val;
                        *ptr_addr = ptr;
                        return;
                    }
                }
            }
            ptr = saved;
            // Fall through if not ON ERROR GOTO, but it should be handled in run_program loop
            // for regular ON index GOTO.
        }
        
        if (t.type == TOKEN_PRINT) {
            int fnum = -1;
            const char *check_ptr = ptr;
            Token first = get_next_token(&check_ptr);
            if (first.type == TOKEN_HASH) {
                ptr = check_ptr;
                fnum = (int)evaluate_expression(&ptr);
                if (fnum < 1 || fnum >= 16 || !file_handles[fnum]) {
                    report_runtime_error(ERR_BAD_FILE_NUMBER);
                    return;
                }
                const char *comma_ptr = ptr;
                Token sep = get_next_token(&comma_ptr);
                if (sep.type == TOKEN_COMMA) ptr = comma_ptr;
            }
            
            int has_args = 0;
            int using_mode = 0;
            int last_was_numeric = 0;
            char using_fmt[256] = "";
            /* detect PRINT USING format: either 'PRINT USING "fmt", expr' or 'PRINT #n, USING "fmt", expr' */
            const char *using_check = ptr;
            Token using_tok = get_next_token(&using_check);
            if (using_tok.type == TOKEN_USING) {
                using_mode = 1;
                ptr = using_check; /* advance past USING */
                parse_string_expression(&ptr, using_fmt, sizeof(using_fmt));

                /* skip optional comma or semicolon after format string */
                const char *sep_check_ptr = ptr;
                Token sep_tok = get_next_token(&sep_check_ptr);
                if (sep_tok.type == TOKEN_COMMA || sep_tok.type == TOKEN_SEMICOLON) {
                    ptr = sep_check_ptr;
                }
            }
            while (1) {
                const char *item_saved = ptr;
                Token next = get_next_token(&ptr);

                if (next.type == TOKEN_EOF || next.type == TOKEN_COLON) {
                    if (!has_args) {
                        ptr = item_saved;
                        if (fnum != -1 && file_handles[fnum]) fprintf(file_handles[fnum], "\n");
                        else basic_output("\n");
                    }
                    break;
                }
                has_args = 1;
                char val_buf[512] = "";
                if (is_string_token(next)) {
                    ptr = item_saved;
                    char value[256] = "";
                    parse_string_expression(&ptr, value, sizeof(value));
                    if (using_mode) apply_basica_using_str(using_fmt, value, val_buf, sizeof(val_buf));
                    else strcpy(val_buf, value);
                    last_was_numeric = 0;
                } else {
                    ptr = item_saved;
                    double val = evaluate_expression(&ptr);
                    if (using_mode) apply_basica_using(using_fmt, val, val_buf, sizeof(val_buf));
                    else {
                        if (val >= 0) snprintf(val_buf, sizeof(val_buf), " %g ", val);
                        else snprintf(val_buf, sizeof(val_buf), "%g ", val);
                    }
                    last_was_numeric = 1;
                }
                if (fnum != -1 && file_handles[fnum]) {
                    fprintf(file_handles[fnum], "%s", val_buf);
                    for (int k = 0; val_buf[k]; k++) {
                        if (val_buf[k] == '\n' || val_buf[k] == '\r') print_col = 0;
                        else print_col++;
                    }
                } else {
                    basic_output(val_buf);
                }

                // Check for separators
                const char *sep_saved = ptr;
                next = get_next_token(&ptr);
                if (next.type == TOKEN_SEMICOLON || next.type == TOKEN_COMMA) {
                    if (next.type == TOKEN_COMMA && !using_mode) {
                        int target = (print_col / 14 + 1) * 14;
                        while (print_col < target) {
                            if (fnum != -1 && file_handles[fnum]) {
                                fprintf(file_handles[fnum], " ");
                                print_col++;
                            } else {
                                basic_output(" ");
                            }
                        }
                    } else if (last_was_numeric && !using_mode) {
                        const char *peek_ptr = ptr;
                        Token peek = get_next_token(&peek_ptr);
                        if (peek.type != TOKEN_EOF && peek.type != TOKEN_COLON && !is_string_token(peek)) {
                            if (fnum != -1 && file_handles[fnum]) {
                                fprintf(file_handles[fnum], " ");
                                print_col++;
                            } else {
                                basic_output(" ");
                            }
                        }
                    }
                } else {
                    ptr = sep_saved;
                    if (fnum != -1 && file_handles[fnum]) {
                        fprintf(file_handles[fnum], "\n");
                        print_col = 0;
                    } else {
                        basic_output("\n");
                    }
                    break;
                }
            }
        } else if (t.type == TOKEN_LET || t.type == TOKEN_IDENTIFIER) {
            Token var_token = (t.type == TOKEN_LET) ? get_next_token(&ptr) : t;
            execute_assignment(&ptr, var_token);
        } else if (t.type == TOKEN_LIST && is_direct) {
            list_program();
        } else if (t.type == TOKEN_RUN && is_direct) {
            run_program();
        } else if (t.type == TOKEN_NEW && is_direct) {
            clear_program();
            clear_variables();
            clear_data_pointer();
            print_col = 0;
        } else if (t.type == TOKEN_BEEP) {
            basic_output("\a");
        } else if (t.type == TOKEN_SOUND) {
            double frequency = evaluate_expression(&ptr);
            Token sep = get_next_token(&ptr);
            if (sep.type == TOKEN_COMMA) {
                double duration = evaluate_expression(&ptr);
                audio_sound(frequency, duration);
            }
        } else if (t.type == TOKEN_PLAY) {
            char mml[256] = "";
            parse_string_expression(&ptr, mml, sizeof(mml));
            audio_play(mml);
        } else if (t.type == TOKEN_DATA) {
            while (*ptr && *ptr != ':') ptr++;
        } else if (t.type == TOKEN_READ) {
            parse_read_list(&ptr);
        } else if (t.type == TOKEN_RESTORE) {
            parse_restore(&ptr);
        } else if (t.type == TOKEN_RANDOMIZE) {
            parse_randomize(&ptr);
        } else if (t.type == TOKEN_DEF) {
            Token fn_tok = get_next_token(&ptr);
            if (fn_tok.type == TOKEN_IDENTIFIER && strncasecmp(fn_tok.text, "FN", 2) == 0) {
                get_next_token(&ptr); // (
                Token param = get_next_token(&ptr);
                get_next_token(&ptr); // )
                get_next_token(&ptr); // =
                
                int f_idx = -1;
                for (int i = 0; i < user_function_count; i++) {
                    if (strcasecmp(user_functions[i].name, fn_tok.text) == 0) { f_idx = i; break; }
                }
                if (f_idx == -1 && user_function_count < 64) f_idx = user_function_count++;
                if (f_idx != -1) {
                    strncpy(user_functions[f_idx].name, fn_tok.text, 31);
                    strncpy(user_functions[f_idx].param_name, param.text, 31);
                    int len = 0;
                    while (ptr[len] && ptr[len] != ':') len++;
                    if (len > 255) len = 255;
                    strncpy(user_functions[f_idx].expression, ptr, len);
                    user_functions[f_idx].expression[len] = '\0';
                    ptr += len;
                }
            }
        } else if (t.type == TOKEN_DEFINT) {
            parse_def_range(&ptr, '%');
        } else if (t.type == TOKEN_DEFSTR) {
            parse_def_range(&ptr, '$');
        } else if (t.type == TOKEN_DEFSNG) {
            parse_def_range(&ptr, '!');
        } else if (t.type == TOKEN_DEFDBL) {
            parse_def_range(&ptr, '#');
        } else if (t.type == TOKEN_FILES) {
            char pattern[256] = "*.bas";
            char redirect_file[256] = "";
            const char *saved = ptr;
            Token pat_tok = get_next_token(&ptr);
            if (pat_tok.type == TOKEN_STRING) {
                strncpy(pattern, pat_tok.text, sizeof(pattern)-1);
                const char *comma_saved = ptr;
                if (get_next_token(&ptr).type == TOKEN_COMMA) {
                    parse_string_expression(&ptr, redirect_file, sizeof(redirect_file));
                } else {
                    ptr = comma_saved;
                }
            } else {
                ptr = saved;
            }
            glob_t results; // Use glob.h for wildcard matching
            int res = glob(pattern, GLOB_NOCHECK, NULL, &results); // GLOB_NOCHECK returns pattern if no match
            if (res == 0) {
                FILE *out = NULL;
                if (redirect_file[0]) out = fopen(redirect_file, "w");
                for (size_t i = 0; i < results.gl_pathc; i++) {
                    struct stat st;
                    char datestr[64] = "";
                    if (stat(results.gl_pathv[i], &st) == 0) {
                        struct tm *tm = localtime(&st.st_mtime);
                        if (tm) strftime(datestr, sizeof(datestr), "%m-%d-%y  %I:%M%p", tm); // two spaces for BASICA-like format
                    }
                    if (out) {
                        fprintf(out, "%s\n", results.gl_pathv[i]);
                    } else {
                        char entry[512];
                        if (S_ISDIR(st.st_mode)) {
                            snprintf(entry, sizeof(entry), "%s %13s %s\n", datestr[0] ? datestr : "", "<DIR>", results.gl_pathv[i]);
                        } else {
                            long long fsize = 0;
                            if (stat(results.gl_pathv[i], &st) == 0) fsize = (long long)st.st_size;
                            snprintf(entry, sizeof(entry), "%s %13lld %s\n", datestr[0] ? datestr : "", fsize, results.gl_pathv[i]);
                        }
                        basic_output(entry);
                    }
                }
                if (out) fclose(out);
                globfree(&results);
            } else {
                basic_output("No files found or error.\n");
            }
        } else if (t.type == TOKEN_CHDIR) {
            char path[256] = "";
            parse_string_expression(&ptr, path, sizeof(path));
            if (chdir(path) != 0) report_runtime_error(ERR_PATH_NOT_FOUND);
        } else if (t.type == TOKEN_MKDIR) {
            char path[256] = "";
            parse_string_expression(&ptr, path, sizeof(path));
            if (mkdir(path, 0777) != 0) report_runtime_error(ERR_PATH_FILE_ACCESS_ERROR);
        } else if (t.type == TOKEN_RMDIR) {
            char path[256] = "";
            parse_string_expression(&ptr, path, sizeof(path));
            if (rmdir(path) != 0) report_runtime_error(ERR_PATH_FILE_ACCESS_ERROR);
        } else if (t.type == TOKEN_SHELL) {
            char cmd[256] = "";
            const char *saved = ptr;
            Token cmd_tok = get_next_token(&ptr);
            if (cmd_tok.type != TOKEN_EOF && cmd_tok.type != TOKEN_COLON) {
                ptr = saved;
                parse_string_expression(&ptr, cmd, sizeof(cmd));
                fflush(stdout);
                if (cmd[0] != '\0') system(cmd);
            } else {
                const char *shell_cmd = getenv("SHELL");
                fflush(stdout);
                system(shell_cmd ? shell_cmd : "sh");
            }
        } else if (t.type == TOKEN_ENVIRON) {
            char cmd[256] = "";
            parse_string_expression(&ptr, cmd, sizeof(cmd));
            if (cmd[0]) {
                putenv(strdup(cmd));
            }
        } else if (t.type == TOKEN_POKE) {
            int addr = (int)evaluate_expression(&ptr);
            const char *saved_comma = ptr;
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                int val = (int)evaluate_expression(&ptr);
                // Ensure address is within bounds before poking
                if (addr >= 0 && addr < 65536) basica_memory[addr] = (unsigned char)(val & 0xFF);
            } else {
                ptr = saved_comma;
            }
        } else if ((t.type == TOKEN_SYSTEM || t.type == TOKEN_QUIT) && is_direct) {
            exit(0);
        } else if (t.type == TOKEN_REM || t.type == TOKEN_APOSTROPHE) { // APOSTROPHE is also a comment
            while (*ptr) ptr++;
        } else if (t.type == TOKEN_SWAP) {
            Token v1_tok = get_next_token(&ptr);
            int idx1 = find_variable(v1_tok.text);
            int a_idx1 = parse_array_index(&ptr, idx1);
            
            get_next_token(&ptr); // consume comma

            Token v2_tok = get_next_token(&ptr);
            int idx2 = find_variable(v2_tok.text);
            int a_idx2 = parse_array_index(&ptr, idx2);

            int is_str1 = (v1_tok.text[0] != '\0' && v1_tok.text[strlen(v1_tok.text)-1] == '$');
            int is_str2 = (v2_tok.text[0] != '\0' && v2_tok.text[strlen(v2_tok.text)-1] == '$');

            if (is_str1 != is_str2) {
                report_runtime_error(ERR_TYPE_MISMATCH);
            } else if (a_idx1 == -1 && a_idx2 == -1) {
                // Swapping entire variables/arrays
                double tmp_val = vars[idx1].value; vars[idx1].value = vars[idx2].value; vars[idx2].value = tmp_val;
                char *tmp_sval = vars[idx1].s_value; vars[idx1].s_value = vars[idx2].s_value; vars[idx2].s_value = tmp_sval;
                double *tmp_arr = vars[idx1].array; vars[idx1].array = vars[idx2].array; vars[idx2].array = tmp_arr;
                char **tmp_sarr = vars[idx1].s_array; vars[idx1].s_array = vars[idx2].s_array; vars[idx2].s_array = tmp_sarr;
                int tmp_size = vars[idx1].array_size; vars[idx1].array_size = vars[idx2].array_size; vars[idx2].array_size = tmp_size;
                int tmp_dims = vars[idx1].num_dims; vars[idx1].num_dims = vars[idx2].num_dims; vars[idx2].num_dims = tmp_dims;
                for (int i = 0; i < 3; i++) {
                    int td = vars[idx1].dims[i]; vars[idx1].dims[i] = vars[idx2].dims[i]; vars[idx2].dims[i] = td;
                }
            } else {
                // Element swapping
                if (is_str1) {
                    char s1[256], s2[256];
                    get_string_variable_value(idx1, a_idx1, s1, sizeof(s1));
                    get_string_variable_value(idx2, a_idx2, s2, sizeof(s2));
                    set_string_variable(idx1, a_idx1, s2);
                    set_string_variable(idx2, a_idx2, s1);
                } else {
                    double val1 = (a_idx1 >= 0 && vars[idx1].array) ? vars[idx1].array[a_idx1] : vars[idx1].value;
                    double val2 = (a_idx2 >= 0 && vars[idx2].array) ? vars[idx2].array[a_idx2] : vars[idx2].value;
                    
                    if (a_idx1 >= 0 && vars[idx1].array) vars[idx1].array[a_idx1] = val2;
                    else vars[idx1].value = val2;
                    
                    if (a_idx2 >= 0 && vars[idx2].array) vars[idx2].array[a_idx2] = val1;
                    else vars[idx2].value = val1;
                }
            }
        } else if (t.type == TOKEN_OPEN) {
            char path_buf[256] = "";
            parse_string_expression(&ptr, path_buf, sizeof(path_buf));
            get_next_token(&ptr); // FOR
            Token mode = get_next_token(&ptr);
            get_next_token(&ptr); // AS
            get_next_token(&ptr); // #
            int fnum = (int)evaluate_expression(&ptr);
            if (fnum >= 1 && fnum < 16) {
                const char *m = mode.text;
                const char *mode_str = "r";
                if (strcasecmp(m, "OUTPUT") == 0) mode_str = "w";
                else if (strcasecmp(m, "INPUT") == 0) mode_str = "r";
                else if (strcasecmp(m, "RANDOM") == 0) mode_str = "w+";
                else if (strcasecmp(m, "RWB") == 0 || strcasecmp(m, "RW") == 0) mode_str = "w+";
                file_handles[fnum] = fopen(path_buf, mode_str);
                if (!file_handles[fnum]) {
                    report_runtime_error(ERR_FILE_NOT_FOUND);
                }
            }
        } else if (t.type == TOKEN_CLOSE) {
            const char *saved = ptr;
            Token hash = get_next_token(&ptr);
            if (hash.type == TOKEN_HASH) {
                int fnum = (int)evaluate_expression(&ptr);
                if (fnum >= 1 && fnum < 16 && file_handles[fnum]) {
                    fclose(file_handles[fnum]);
                    file_handles[fnum] = NULL;
                }
            } else {
                ptr = saved;
            }
        } else if (t.type == TOKEN_KILL) {
            char path_buf[256] = "";
            parse_string_expression(&ptr, path_buf, sizeof(path_buf));
            if (path_buf[0]) {
                glob_t results;
                int glob_result = glob(path_buf, 0, NULL, &results);
                int deleted_any = 0;
                if (glob_result == 0) {
                    for (size_t i = 0; i < results.gl_pathc; i++) {
                        if (remove(results.gl_pathv[i]) == 0) {
                            deleted_any = 1;
                        } else {
                            char err[256];
                            snprintf(err, sizeof(err), "Delete failed: %s\n", results.gl_pathv[i]);
                            basic_output(err);
                        }
                    }
                    globfree(&results);
                }
                if (deleted_any) {
                    basic_output("Deleted\n");
                } else if (glob_result == GLOB_NOMATCH) {
                    char err[256];
                    snprintf(err, sizeof(err), "Delete failed: %s\n", path_buf);
                    basic_output(err);
                }
            }
        } else if (t.type == TOKEN_DRAW) {
            char cmd[1024] = "";
            parse_string_expression(&ptr, cmd, sizeof(cmd));
            const char *c = cmd;
            int x, y;
            get_graphics_cursor(&x, &y);
            int color = 7; 
            int scale = 4;
            int angle = 0; // 0, 1, 2, 3 -> 0, 90, 180, 270 degrees
            int ta = 0; // Turn Angle in degrees

            while (*c) {
                while (*c && (isspace((unsigned char)*c) || *c == ';' || *c == ',')) c++;
                if (!*c) break;

                int no_draw = 0;
                int return_pos = 0;
                if (toupper((unsigned char)*c) == 'B') { no_draw = 1; c++; }
                if (toupper((unsigned char)*c) == 'N') { return_pos = 1; c++; }
                
                char op = toupper((unsigned char)*c++);
                int orig_x = x, orig_y = y;

                while (*c && (isspace((unsigned char)*c) || *c == ';' || *c == ',')) c++;

                int val = 0;
                if (op != 'M' && (isdigit((unsigned char)*c) || *c == '+' || *c == '-')) {
                    char *end;
                    val = strtol(c, &end, 10);
                    c = end;
                }

                int dx = 0, dy = 0;
                switch (op) {
                    case 'U': dy = -val; break;
                    case 'D': dy = val; break;
                    case 'L': dx = -val; break;
                    case 'R': dx = val; break;
                    case 'E': dx = val; dy = -val; break;
                    case 'F': dx = val; dy = val; break;
                    case 'G': dx = -val; dy = val; break;
                    case 'H': dx = -val; dy = -val; break;
                    case 'M': {
                        int rel = 0;
                        if (*c == '+' || *c == '-') rel = 1;
                        int mx = strtol(c, (char**)&c, 10);
                        if (*c == ',') c++;
                        int my = strtol(c, (char**)&c, 10);
                        if (rel) {
                            dx = mx;
                            dy = my;
                        } else {
                            x = mx;
                            y = my;
                            if (!no_draw) draw_line(orig_x, orig_y, x, y, color, 0);
                            if (return_pos) { x = orig_x; y = orig_y; }
                            set_graphics_cursor(x, y);
                            continue;
                        }
                        break;
                    }
                    case 'A': angle = val % 4; break;
                    case 'T': if (toupper((unsigned char)*c) == 'A') { c++; ta = strtol(c, (char**)&c, 10); } break;
                    case 'S': scale = val; break;
                    case 'C': color = val; break;
                    case 'P': {
                        int p_color = val;
                        int b_color = color;
                        if (*c == ',') { c++; b_color = strtol(c, (char**)&c, 10); }
                        draw_paint(x, y, p_color, b_color);
                        continue;
                    }
                }

                if (dx != 0 || dy != 0) {
                    dx = dx * scale / 4;
                    dy = dy * scale / 4;
                    
                    if (angle == 1) { int t = dx; dx = -dy; dy = t; }
                    else if (angle == 2) { dx = -dx; dy = -dy; }
                    else if (angle == 3) { int t = dx; dx = dy; dy = -t; }

                    if (ta != 0) {
                        double rad = ta * M_PI / 180.0;
                        int nx = (int)(dx * cos(rad) - dy * sin(rad));
                        int ny = (int)(dx * sin(rad) + dy * cos(rad));
                        dx = nx; dy = ny;
                    }

                    x += dx; y += dy;
                    if (!no_draw) draw_line(orig_x, orig_y, x, y, color, 0);
                    if (return_pos) { x = orig_x; y = orig_y; }
                    set_graphics_cursor(x, y);
                }
            }
        } else if (t.type == TOKEN_NAME) {
            char old_name[256] = "";
            char new_name[256] = "";
            parse_string_expression(&ptr, old_name, sizeof(old_name));
            Token as_tok = get_next_token(&ptr);
            if (as_tok.type != TOKEN_AS) {
                report_runtime_error(ERR_SYNTAX_ERROR);
            } else {
                parse_string_expression(&ptr, new_name, sizeof(new_name));
                if (rename(old_name, new_name) != 0) {
                    report_runtime_error(ERR_PATH_FILE_ACCESS_ERROR);
                }
            }
        } else if (t.type == TOKEN_DELETE) {
            // DELETE "file"  or DELETE <line_number>
            Token arg = get_next_token(&ptr);
            if (arg.type == TOKEN_STRING) {
                if (remove(arg.text) == 0) {
                    basic_output("Deleted\n");
                } else {
                    char err[256];
                    snprintf(err, sizeof(err), "Delete failed: %s\n", arg.text);
                    basic_output(err);
                }
            } else if (arg.type == TOKEN_NUMBER) {
                delete_line(arg.int_val);
            }
        } else if (t.type == TOKEN_ERASE) {
            // ERASE var or ERASE var$(index)
            Token var = get_next_token(&ptr);
            if (var.type == TOKEN_IDENTIFIER) {
                int idx = find_variable(var.text);
                // Check for array index
                int array_idx = -1;
                const char *check_ptr = ptr;
                if (get_next_token(&check_ptr).type == TOKEN_LPAREN) {
                    array_idx = parse_array_index(&ptr, idx);
                } // No else, array_idx remains -1 if no paren
                if (var.text[strlen(var.text)-1] == '$') {
                    // erase string variable or string array element
                    if (array_idx >= 0 && vars[idx].s_array && array_idx < vars[idx].array_size) {
                        if (vars[idx].s_array[array_idx]) free(vars[idx].s_array[array_idx]);
                        vars[idx].s_array[array_idx] = NULL;
                    } else if (vars[idx].s_array) {
                        // ERASE S$ with no index on a string array: release all memory
                        for (int i = 0; i < vars[idx].array_size; i++) {
                            if (vars[idx].s_array[i]) {
                                free(vars[idx].s_array[i]);
                            }
                        }
                        free(vars[idx].s_array);
                        vars[idx].s_array = NULL;
                        vars[idx].array_size = 0;
                        vars[idx].num_dims = 0;
                    } else if (array_idx == -1) { // Scalar string variable
                        if (vars[idx].s_value) free(vars[idx].s_value);
                        vars[idx].s_value = NULL;
                    }
                } else {
                    // numeric variable or array
                    if (array_idx >= 0 && vars[idx].array && array_idx < vars[idx].array_size) {
                        vars[idx].array[array_idx] = 0;
                    } else if (vars[idx].array) {
                        // ERASE A with no index: release numeric array memory
                        free(vars[idx].array);
                        vars[idx].array = NULL;
                        vars[idx].array_size = 0;
                        vars[idx].num_dims = 0;
                    } else if (array_idx == -1) { // Scalar numeric variable
                        vars[idx].value = 0;
                    }
                }
            }
            
        } else if (t.type == TOKEN_COLOR) {
            int color_value = (int)evaluate_expression(&ptr);
            const char *saved = ptr;
            Token sep = get_next_token(&ptr);
            if (sep.type == TOKEN_COMMA) {
                (void)evaluate_expression(&ptr); // optional background color ignored for now
            } else {
                ptr = saved;
            }
            set_text_color(color_value); // Set foreground color
        } else if (t.type == TOKEN_INPUT) {
            const char *saved = ptr;
            int suppress_question = 0;
            Token prompt_tok = get_next_token(&ptr);
            if (prompt_tok.type == TOKEN_STRING) {
                if (graphics_is_active()) graphics_print(prompt_tok.text);
                else printf("%s", prompt_tok.text);
                Token sep = get_next_token(&ptr);
                if (sep.type == TOKEN_COMMA) {
                    suppress_question = 1;
                } else if (sep.type != TOKEN_SEMICOLON) {
                    ptr = saved; // not a real prompt, rewind
                }
            } else {
                ptr = saved;
            }

            saved = ptr;
            Token hash = get_next_token(&ptr);
            int fnum = -1;
            if (hash.type == TOKEN_HASH) {
                fnum = (int)evaluate_expression(&ptr);
                const char *comma_ptr = ptr;
                Token sep = get_next_token(&comma_ptr);
                if (sep.type == TOKEN_COMMA) ptr = comma_ptr;
            } else {
                ptr = saved;
            }

            int input_var_count = 0;
            int var_indices[16];
            int var_array_idx[16];
            int var_is_str[16];
            while (input_var_count < 16) {
                const char *var_saved = ptr;
                Token var = get_next_token(&ptr);
                if (var.type != TOKEN_IDENTIFIER) {
                    ptr = var_saved;
                    break;
                }
                var_indices[input_var_count] = find_variable(var.text);
                var_array_idx[input_var_count] = parse_array_index(&ptr, var_indices[input_var_count]);
                var_is_str[input_var_count] = (var.text[strlen(var.text) - 1] == '$');
                input_var_count++;
                const char *sep_saved = ptr;
                Token sep = get_next_token(&ptr);
                if (sep.type != TOKEN_COMMA && sep.type != TOKEN_SEMICOLON) {
                    ptr = sep_saved;
                    break;
                }
            }
            if (input_var_count == 0) return;

            char line[256] = "";
            if (fnum != -1) {
                if (fnum < 1 || fnum >= 16 || !file_handles[fnum]) {
                    report_runtime_error(ERR_BAD_FILE_NUMBER);
                } else if (fgets(line, sizeof(line), file_handles[fnum])) {
                    line[strcspn(line, "\r\n")] = 0;
                }
            } else {
                if (!suppress_question) basic_output("? ");
                if (graphics_is_active()) {
                    graphics_readline(line, sizeof(line));
                } else if (fgets(line, sizeof(line), stdin)) {
                    line[strcspn(line, "\r\n")] = 0;
                }
            }

            char *saveptr = NULL;
            char *value_token = strtok_r(line, ",;", &saveptr);
            for (int i = 0; i < input_var_count; i++) {
                char value[256] = "";
                if (value_token) {
                    strncpy(value, value_token, sizeof(value) - 1);
                    value[sizeof(value) - 1] = '\0';
                    trim_string(value);
                    value_token = strtok_r(NULL, ",;", &saveptr);
                }
                assign_input_value(var_indices[i], var_array_idx[i], var_is_str[i], value);
            }
        } else if (t.type == TOKEN_REVERSE) {
            Token var = get_next_token(&ptr);
            int idx = find_variable(var.text);
            int array_idx = parse_array_index(&ptr, idx);
            char *target = NULL;
            if (array_idx != -1 && vars[idx].s_array && array_idx >= 0 && array_idx < vars[idx].array_size) {
                target = vars[idx].s_array[array_idx];
            } else {
                target = vars[idx].s_value;
            }
            if (target) {
                size_t len = strlen(target);
                char *rev = malloc(len + 1);
                for (size_t i = 0; i < len; i++) rev[i] = target[len - 1 - i];
                rev[len] = '\0';
                if (array_idx != -1 && vars[idx].s_array && array_idx >= 0 && array_idx < vars[idx].array_size) {
                    free(vars[idx].s_array[array_idx]);
                    vars[idx].s_array[array_idx] = rev;
                } else {
                    if (vars[idx].s_value) free(vars[idx].s_value);
                    vars[idx].s_value = rev;
                }
            }
        } else if (t.type == TOKEN_OPTION) {
            Token base = get_next_token(&ptr);
            if (base.type != TOKEN_BASE) {
                report_runtime_error(ERR_SYNTAX_ERROR);
            } else {
                Token val_tok = get_next_token(&ptr);
                if (val_tok.type != TOKEN_NUMBER) {
                    report_runtime_error(ERR_SYNTAX_ERROR);
                } else {
                    int val = val_tok.int_val;
                    if (val != 0 && val != 1) {
                        report_runtime_error(ERR_ILLEGAL_FUNCTION_CALL);
                    } else if (option_base_set || arrays_dimensioned) {
                        report_runtime_error(ERR_DUPLICATE_DEFINITION);
                    } else {
                        option_base = val;
                        option_base_set = 1;
                    }
                }
            }
        } else if (t.type == TOKEN_DIM) {
            // Process multiple DIM declarations (e.g., DIM A(2,2), B(3,3), C(2))
            while (1) {
                Token var = get_next_token(&ptr);
                if (var.type != TOKEN_IDENTIFIER) break;
                
                Token lparen = get_next_token(&ptr);
                if (lparen.type != TOKEN_LPAREN) break;
                
                // Parse dimensions (can be multiple, separated by commas)
                int dims[3] = {0};
                int num_dims = 0;
                dims[0] = (int)evaluate_expression(&ptr);
                num_dims = 1;
                
                // Check for additional dimensions
                const char *saved = ptr;
                Token sep = get_next_token(&ptr);
                while (sep.type == TOKEN_COMMA && num_dims < 3) {
                    dims[num_dims] = (int)evaluate_expression(&ptr);
                    num_dims++;
                    saved = ptr;
                    sep = get_next_token(&ptr);
                }
                ptr = saved;
                Token rparen = get_next_token(&ptr);
                if (rparen.type != TOKEN_RPAREN) break;
                
                int idx = find_variable(var.text);
                
                // Calculate total size and check if any dim is invalid
                int total_size = 1;
                int dim_err = 0;
                for (int i = 0; i < num_dims; i++) {
                    int dim_size = (option_base == 0) ? (dims[i] + 1) : dims[i];
                    if (dim_size <= 0) {
                        report_runtime_error(ERR_SUBSCRIPT_OUT_OF_RANGE);
                        dim_err = 1;
                        break;
                    }
                    total_size *= dim_size;
                }
                if (dim_err) break;
                
                if (var.text[strlen(var.text)-1] == '$') {
                    if (vars[idx].s_array) {
                        report_runtime_error(ERR_DUPLICATE_DEFINITION);
                        break;
                    }
                    vars[idx].s_array = calloc(total_size, sizeof(char*));
                } else {
                    if (vars[idx].array) {
                        report_runtime_error(ERR_DUPLICATE_DEFINITION);
                        break;
                    }
                    vars[idx].array = malloc(total_size * sizeof(double));
                    for(int i=0; i<total_size; i++) vars[idx].array[i] = 0;
                }
                vars[idx].array_size = total_size;
                vars[idx].num_dims = num_dims;
                for(int i=0; i<num_dims; i++) vars[idx].dims[i] = dims[i];
                
                arrays_dimensioned = 1;
                
                // Check for more arrays on the same DIM statement
                saved = ptr;
                Token comma = get_next_token(&ptr);
                if (comma.type != TOKEN_COMMA) {
                    ptr = saved;
                    break;
                }
            }
        } else if (t.type == TOKEN_PSET) {
            int x, y;
            const char *saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_LPAREN) {
                x = (int)evaluate_expression(&ptr);
                if (get_next_token(&ptr).type == TOKEN_COMMA) {
                    y = (int)evaluate_expression(&ptr);
                } else {
                    report_runtime_error(ERR_SYNTAX_ERROR);
                    return;
                }
                if (get_next_token(&ptr).type != TOKEN_RPAREN) {
                    report_runtime_error(ERR_SYNTAX_ERROR);
                    return;
                }
            } else {
                ptr = saved;
                x = (int)evaluate_expression(&ptr);
                if (get_next_token(&ptr).type == TOKEN_COMMA) {
                    y = (int)evaluate_expression(&ptr);
                } else {
                    report_runtime_error(ERR_SYNTAX_ERROR);
                    return;
                }
            }
            int col = 7;
            saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                col = (int)evaluate_expression(&ptr);
            } else {
                ptr = saved;
            }
            set_pixel(x, y, col);
            update_graphics();
        } else if (t.type == TOKEN_LINE) {
            int x1, y1, x2, y2;
            int has_first = 0;
            const char *saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_LPAREN) {
                x1 = (int)evaluate_expression(&ptr);
                if (get_next_token(&ptr).type == TOKEN_COMMA) y1 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // RPAREN
                has_first = 1;
            } else {
                ptr = saved;
                // Check if it starts with - (relative line)
                if (get_next_token(&ptr).type == TOKEN_MINUS) {
                    int dummy_x, dummy_y;
                    get_graphics_cursor(&dummy_x, &dummy_y);
                    x1 = dummy_x; y1 = dummy_y;
                    has_first = 1;
                } else {
                    ptr = saved;
                }
            }
            
            if (has_first) {
                if (get_next_token(&ptr).type != TOKEN_MINUS) {
                    // If it was (x1,y1) but no - next, maybe it's just (x1,y1)-(x2,y2) 
                    // and evaluate_expression consumed more? No, evaluate_expression is clean.
                }
            } else {
                // Should be (x1,y1)-(x2,y2) or x1,y1-x2,y2
                x1 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                y1 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // minus
            }

            saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_LPAREN) {
                x2 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                y2 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // RPAREN
            } else {
                ptr = saved;
                x2 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                y2 = (int)evaluate_expression(&ptr);
            }

            int col = 3;
            saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                col = (int)evaluate_expression(&ptr);
            } else {
                ptr = saved;
            }

            int fill = 0;
            saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                Token flag = get_next_token(&ptr);
                if (strcasecmp(flag.text, "BF") == 0) fill = 2;
                else if (strcasecmp(flag.text, "B") == 0) fill = 1;
                else ptr = saved;
            } else {
                ptr = saved;
            }
            draw_line(x1, y1, x2, y2, col, fill);
            update_graphics();
        } else if (t.type == TOKEN_CIRCLE) { // CIRCLE (cx,cy),radius[,color]
            int cx, cy, radius;
            const char *saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_LPAREN) {
                cx = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                cy = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // RPAREN
            } else {
                ptr = saved;
                cx = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                cy = (int)evaluate_expression(&ptr);
            }
            get_next_token(&ptr); // comma
            radius = (int)evaluate_expression(&ptr);

            int col = 3;
            saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                col = (int)evaluate_expression(&ptr);
            } else {
                ptr = saved;
            }
            int fill = 0;
            saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                fill = (int)evaluate_expression(&ptr);
            } else {
                ptr = saved;
            }
            draw_circle(cx, cy, radius, col, fill);
            update_graphics();
        } else if (t.type == TOKEN_PAINT) { // PAINT (x,y)[,color[,border]]
            int x, y;
            const char *saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_LPAREN) {
                x = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                y = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // RPAREN
            } else {
                ptr = saved;
                x = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                y = (int)evaluate_expression(&ptr);
            }
            int col = 3;
            saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                col = (int)evaluate_expression(&ptr);
            } else {
                ptr = saved;
            }
            int border = col;
            saved = ptr;
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                border = (int)evaluate_expression(&ptr);
            } else {
                ptr = saved;
            }
            draw_paint(x, y, col, border);
            update_graphics();
        } else if (t.type == TOKEN_SCREEN) {
            int mode = (int)evaluate_expression(&ptr);
            init_graphics();
            set_screen_mode(mode); 
        } else if (t.type == TOKEN_LOCATE) {
            int row = (int)evaluate_expression(&ptr);
            if (get_next_token(&ptr).type == TOKEN_COMMA) {
                int col = (int)evaluate_expression(&ptr);
                set_text_cursor(row, col);
                print_col = col - 1;
            }
        } else if (t.type == TOKEN_CLS) {
            graphics_cls();
            if (graphics_is_active()) graphics_present_now();
            print_col = 0;
        } else if (t.type == TOKEN_SLEEP) {
            int ms = (int)evaluate_expression(&ptr);
            graphics_sleep(ms);
        }
        else if (t.type == TOKEN_SEEK) {
            const char *saved = ptr;
            Token hash = get_next_token(&ptr);
            int fnum = -1;
            if (hash.type == TOKEN_HASH) {
                fnum = (int)evaluate_expression(&ptr);
                Token sep = get_next_token(&ptr);
                if (sep.type == TOKEN_COMMA) {
                    double pos = evaluate_expression(&ptr);
                    if (fnum >= 1 && fnum < 16 && file_handles[fnum]) {
                        fseek(file_handles[fnum], (long)pos, SEEK_SET);
                    } else {
                        report_runtime_error(ERR_BAD_FILE_NUMBER);
                    }
                } else {
                    ptr = saved;
                }
            } else {
                ptr = saved;
            }
        }
        else if (t.type == TOKEN_PUT) {
            const char *saved = ptr;
            Token next = get_next_token(&ptr);
            if (next.type == TOKEN_LPAREN) {
                // Graphics PUT (x,y), array [, action]
                int x = (int)evaluate_expression(&ptr);
                if (get_next_token(&ptr).type == TOKEN_COMMA) {
                    int y = (int)evaluate_expression(&ptr);
                    get_next_token(&ptr); // RPAREN
                    get_next_token(&ptr); // comma
                    Token var_tok = get_next_token(&ptr);
                    int idx = find_variable(var_tok.text);
                    
                    if (idx != -1 && vars[idx].array && vars[idx].array_size >= 2) {
                        int w = (int)vars[idx].array[0];
                        int h = (int)vars[idx].array[1];
                        int action = 0; // 0=XOR, 1=PSET, 2=PRESET, 3=AND, 4=OR
                        const char *action_saved = ptr;
                        if (get_next_token(&ptr).type == TOKEN_COMMA) {
                            Token act_tok = get_next_token(&ptr);
                            if (act_tok.type == TOKEN_PSET || strcasecmp(act_tok.text, "PSET") == 0) action = 1;
                            else if (strcasecmp(act_tok.text, "PRESET") == 0) action = 2;
                            else if (strcasecmp(act_tok.text, "AND") == 0) action = 3;
                            else if (strcasecmp(act_tok.text, "OR") == 0) action = 4;
                            else if (strcasecmp(act_tok.text, "XOR") == 0) action = 0;
                        } else { ptr = action_saved; }

                        int k = 2;
                        for (int j = 0; j < h; j++) {
                            for (int i = 0; i < w; i++) {
                                if (k >= vars[idx].array_size) break;
                                int src_col = (int)vars[idx].array[k++];
                                int dst_col = get_pixel(x + i, y + j);
                                int final_col = src_col;
                                switch(action) {
                                    case 0: final_col = src_col ^ dst_col; break;
                                    case 1: final_col = src_col; break;
                                    case 2: final_col = ~src_col & 0x0F; break;
                                    case 3: final_col = src_col & dst_col; break;
                                    case 4: final_col = src_col | dst_col; break;
                                }
                                set_pixel(x + i, y + j, final_col);
                            }
                        }
                        if (graphics_is_active()) graphics_present_now();
                    }
                }
            } else if (next.type == TOKEN_HASH) {
                int fnum = (int)evaluate_expression(&ptr);
                Token sep = get_next_token(&ptr);
                if (sep.type != TOKEN_COMMA) { ptr = saved; }
                else {
                    double rec = evaluate_expression(&ptr);
                    sep = get_next_token(&ptr);
                    if (sep.type != TOKEN_COMMA) { ptr = saved; }
                    else {
                        int len = (int)evaluate_expression(&ptr);
                        sep = get_next_token(&ptr);
                        char data[512] = "";
                        if (sep.type == TOKEN_COMMA) {
                            const char *expr_saved = ptr;
                            Token tok = get_next_token(&ptr);
                            if (tok.type == TOKEN_STRING) {
                                strncpy(data, tok.text, sizeof(data)-1);
                            } else if (tok.type == TOKEN_IDENTIFIER && tok.text[strlen(tok.text)-1] == '$') {
                                int src_idx = find_variable(tok.text);
                                int src_array_idx = parse_array_index(&ptr, src_idx);
                                char temp[512] = "";
                                get_string_variable_value(src_idx, src_array_idx, temp, sizeof(temp));
                                strncpy(data, temp, sizeof(data)-1);
                            } else {
                                ptr = expr_saved;
                                parse_string_expression(&ptr, data, sizeof(data));
                            }
                        } else {
                            ptr = saved;
                        }

                        if (fnum < 1 || fnum >= 16 || !file_handles[fnum]) {
                            report_runtime_error(ERR_BAD_FILE_NUMBER);
                        } else {
                            
                            long offset = (long)((rec - 1) * len);
                            fseek(file_handles[fnum], offset, SEEK_SET);
                            char buf[512];
                            memset(buf, ' ', len);
                            strncpy(buf, data, len);
                            fwrite(buf, 1, len, file_handles[fnum]);
                            fflush(file_handles[fnum]);
                        }
                    }
                }
            } else {
                ptr = saved;
                report_runtime_error(ERR_SYNTAX_ERROR);
            }
        }
        else if (t.type == TOKEN_GET) {
            const char *saved = ptr;
            Token next = get_next_token(&ptr);
            if (next.type == TOKEN_LPAREN) {
                // Graphics GET (x1,y1)-(x2,y2), array
                int x1 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                int y1 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // RPAREN
                get_next_token(&ptr); // minus
                get_next_token(&ptr); // LPAREN
                int x2 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // comma
                int y2 = (int)evaluate_expression(&ptr);
                get_next_token(&ptr); // RPAREN
                get_next_token(&ptr); // comma
                Token var_tok = get_next_token(&ptr);
                int idx = find_variable(var_tok.text);
                
                int w = abs(x2 - x1) + 1;
                int h = abs(y2 - y1) + 1;
                int min_x = (x1 < x2) ? x1 : x2;
                int min_y = (y1 < y2) ? y1 : y2;
                int total_size = 2 + w * h;
                if (idx != -1 && vars[idx].array && vars[idx].array_size >= total_size) {
                    vars[idx].array[0] = (double)w;
                    vars[idx].array[1] = (double)h;
                    int k = 2;
                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            vars[idx].array[k++] = (double)get_pixel(min_x + i, min_y + j);
                        }
                    }
                } else { report_runtime_error(ERR_SUBSCRIPT_OUT_OF_RANGE); }
            } else if (next.type == TOKEN_HASH) {
                int fnum = (int)evaluate_expression(&ptr);
                Token sep = get_next_token(&ptr);
                if (sep.type != TOKEN_COMMA) { ptr = saved; }
                else {
                    double rec = evaluate_expression(&ptr);
                    sep = get_next_token(&ptr);
                    if (sep.type != TOKEN_COMMA) { ptr = saved; }
                    else {
                        int len = (int)evaluate_expression(&ptr);
                        sep = get_next_token(&ptr);
                        if (sep.type != TOKEN_COMMA) { ptr = saved; }
                        else {
                            Token var = get_next_token(&ptr);
                            if (!(var.type == TOKEN_IDENTIFIER && var.text[strlen(var.text)-1] == '$')) { ptr = saved; }
                            else {
                                int idx = find_variable(var.text);
                                int array_idx = parse_array_index(&ptr, idx);
                                if (fnum < 1 || fnum >= 16 || !file_handles[fnum]) {
                                    report_runtime_error(ERR_BAD_FILE_NUMBER);
                                } else {
                                    long offset = (long)((rec - 1) * len);
                                    fseek(file_handles[fnum], offset, SEEK_SET);
                                    char buf[512];
                                    size_t r = fread(buf, 1, len, file_handles[fnum]);
                                    if (r < (size_t)len) {
                                        for (size_t i=r;i<(size_t)len;i++) buf[i] = ' ';
                                    }
                                    buf[len] = '\0';
                                    // Trim trailing spaces
                                    int trim = len - 1;
                                    while (trim >= 0 && buf[trim] == ' ') { buf[trim] = '\0'; trim--; }
                                    if (array_idx >= 0 && vars[idx].s_array && array_idx >= 0 && array_idx < vars[idx].array_size) {
                                        if (vars[idx].s_array[array_idx]) free(vars[idx].s_array[array_idx]);
                                        vars[idx].s_array[array_idx] = strdup(buf);
                                    } else {
                                        if (vars[idx].s_value) free(vars[idx].s_value);
                                        vars[idx].s_value = strdup(buf);
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                ptr = saved;
                report_runtime_error(ERR_SYNTAX_ERROR);
            }
        } else if (t.type == TOKEN_SCREENSHOT) {
            char filename[256] = "";
            if (!parse_string_expression(&ptr, filename, sizeof(filename))) {
                report_runtime_error(ERR_SYNTAX_ERROR);
                return;
            }
            if (!graphics_is_active()) {
                report_runtime_error(ERR_ILLEGAL_FUNCTION_CALL);
            } else {
                if (!graphics_save_screenshot(filename)) {
                    report_runtime_error(ERR_ILLEGAL_FUNCTION_CALL);
                }
            }
        } else {
            // Token was not a recognized command or valid assignment
            report_runtime_error(ERR_SYNTAX_ERROR);
        }
    }
    *ptr_addr = ptr;
}

void interpret_line(const char *input, int is_direct) {
    const char *ptr = input;
    interpret_line_at_ptr(&ptr, is_direct);
}

void run_program() {
    clear_variables();
    stop_running = 0;
    runtime_error_occurred = 0;
    gosub_ptr = 0;
    while_ptr = 0;
    for_ptr = 0;
    print_col = 0;
    clear_data_pointer();
    Statement *curr = get_head();
    const char *resume_ptr = NULL;
    error_stmt = NULL;
    error_ptr = NULL;
    error_next_ptr = NULL;

    while (curr && !stop_running) {
        handle_events();
        Statement *exec_stmt = curr;
        current_executing_line = exec_stmt->line_number;
        const char *ptr = (resume_ptr) ? resume_ptr : exec_stmt->raw_command;
        const char *stmt_start = ptr;
        resume_ptr = NULL;
        int jumped = 0;

        while (*ptr && !stop_running) {
            stmt_start = ptr;
            const char *exec_start = stmt_start;
            Token t = get_next_token(&ptr);
            if (t.type == TOKEN_COLON) continue;

            if (t.type == TOKEN_IF) {
                double cond_val = evaluate_expression(&ptr);
                Token then_tok = get_next_token(&ptr);
                if (then_tok.type != TOKEN_THEN && then_tok.type != TOKEN_GOTO) {
                    report_runtime_error(ERR_SYNTAX_ERROR);
                    break;
                }

                const char *else_ptr = NULL;
                const char *scan = ptr;
                int depth = 0;
                while (*scan) {
                    const char *saved_scan = scan;
                    Token tok = get_next_token(&scan);
                    if (tok.type == TOKEN_LPAREN) depth++;
                    else if (tok.type == TOKEN_RPAREN && depth > 0) depth--;
                    else if (tok.type == TOKEN_IF) depth++;
                    else if (tok.type == TOKEN_ELSE) {
                        if (depth == 0) {
                            else_ptr = saved_scan;
                            break;
                        }
                        depth--;
                    }
                }

                if (cond_val != 0) {
                    // Condition is true: execute THEN part
                    // Check for immediate jump (e.g., IF cond THEN 100 or IF cond GOTO 100)
                    const char *check_ptr = ptr;
                    Token first = get_next_token(&check_ptr);
                    if (first.type == TOKEN_NUMBER) {
                        Statement *target = find_line(first.int_val);
                        if (!target) {
                            report_runtime_error(ERR_UNDEFINED_LINE_NUMBER);
                        } else {
                            curr = target;
                            jumped = 1;
                        }
                        break;
                    }
                    if (first.type == TOKEN_GOTO) {
                        Token target = get_next_token(&check_ptr);
                        if (target.type == TOKEN_NUMBER) {
                            Statement *target_stmt = find_line(target.int_val);
                            if (!target_stmt) {
                                report_runtime_error(ERR_UNDEFINED_LINE_NUMBER);
                            } else {
                                curr = target_stmt;
                                jumped = 1;
                            }
                            break;
                        }
                        ptr = check_ptr; // Continue after GOTO
                    }
                    // Otherwise, just let the loop continue from current ptr (after THEN)
                    continue;
                } else {
                    // Condition is false
                    if (else_ptr) {
                        ptr = else_ptr;
                        get_next_token(&ptr); // consume ELSE
                        continue;
                    } else {
                        // Skip rest of line
                        ptr = "";
                        break;
                    }
                }
            }
            
            if (t.type == TOKEN_FOR) {
                Token var = get_next_token(&ptr);
                Token eq = get_next_token(&ptr);
                if (eq.type != TOKEN_EQUALS) {
                    report_runtime_error(ERR_SYNTAX_ERROR);
                    break;
                }
                double start = evaluate_expression(&ptr);
                get_next_token(&ptr); // TO
                double end = evaluate_expression(&ptr);
                double step = 1.0;
                const char *saved = ptr;
                Token st = get_next_token(&ptr);
                if (st.type == TOKEN_STEP) step = evaluate_expression(&ptr);
                else ptr = saved;
                int idx = find_variable(var.text);
                vars[idx].value = start;

                // BASICA check: If the loop should not execute at all
                int should_skip = 0;
                if (step > 0 && start > end) should_skip = 1;
                else if (step < 0 && start < end) should_skip = 1;

                if (should_skip) {
                    int nest = 1;
                    while (nest > 0 && exec_stmt) {
                        while (*ptr) {
                            Token skip_t = get_next_token(&ptr);
                            if (skip_t.type == TOKEN_FOR) nest++;
                            else if (skip_t.type == TOKEN_NEXT) {
                                Token next_var = get_next_token(&ptr);
                                if (next_var.type == TOKEN_IDENTIFIER && strcasecmp(next_var.text, var.text) == 0) {
                                    nest--;
                                } else if (next_var.type == TOKEN_EOF || next_var.type == TOKEN_COLON) {
                                    nest--; // NEXT without variable matches current FOR
                                }
                            }
                            if (nest == 0) {
                                break;
                            }
                        }
                        if (nest > 0) {
                            exec_stmt = find_next_statement(exec_stmt->line_number);
                            if (exec_stmt) {
                                ptr = exec_stmt->raw_command;
                            }
                        }
                    }
                    curr = exec_stmt;
                    resume_ptr = ptr;
                    jumped = 1;
                    break; // Exit the command loop for this line
                }

                if (for_ptr < 16) {
                    for_stack[for_ptr].var_idx = idx;
                    for_stack[for_ptr].end_val = end;
                    for_stack[for_ptr].step_val = step;
                    for_stack[for_ptr].start_stmt = exec_stmt;
                    for_stack[for_ptr].start_ptr = ptr;
                    for_ptr++;
                } else {
                    report_runtime_error(ERR_OUT_OF_MEMORY);
                }
                continue;
            }
            
            if (t.type == TOKEN_WHILE) {
                const char *cond_start = stmt_start;
                double val = evaluate_expression(&ptr);
                if (val != 0) {
                    // Push only if this WHILE is not already the current active loop
                    if (while_ptr == 0 || while_stack[while_ptr-1].stmt != exec_stmt || while_stack[while_ptr-1].ptr != cond_start) {
                        if (while_ptr < 16) {
                            while_stack[while_ptr].stmt = exec_stmt;
                            while_stack[while_ptr].ptr = cond_start;
                            while_ptr++;
                        } else {
                            report_runtime_error(ERR_OUT_OF_MEMORY);
                        }
                    }
                } else {
                    // Condition is false; pop the loop from stack if we were in it
                    if (while_ptr > 0 && while_stack[while_ptr-1].stmt == exec_stmt && while_stack[while_ptr-1].ptr == cond_start) {
                        while_ptr--;
                    }
                    int nest = 1;
                    while (nest > 0 && exec_stmt) {
                        while (*ptr) {
                            Token skip_t = get_next_token(&ptr);
                            if (skip_t.type == TOKEN_WHILE) nest++;
                            else if (skip_t.type == TOKEN_WEND) nest--;
                            if (nest == 0) {
                                break;
                            }
                        }
                        if (nest > 0) {
                            exec_stmt = find_next_statement(exec_stmt->line_number);
                            if (exec_stmt) {
                                ptr = exec_stmt->raw_command;
                            }
                        }
                    }
                    curr = exec_stmt;
                    resume_ptr = ptr;
                    jumped = 1;
                    break; // Exit command loop for the WHILE line
                }
                continue;
            }

            if (t.type == TOKEN_WEND) {
                if (while_ptr > 0) {
                    curr = while_stack[while_ptr-1].stmt;
                    resume_ptr = while_stack[while_ptr-1].ptr;
                    jumped = 1;
                    break;
                } else {
                    report_runtime_error(ERR_WEND_WITHOUT_WHILE);
                    break;
                }
            }


            if (t.type == TOKEN_ON) {
                const char *on_ptr = ptr;
                Token next_on = get_next_token(&on_ptr);
                if (next_on.type == TOKEN_ERR || (next_on.type == TOKEN_IDENTIFIER && strcasecmp(next_on.text, "ERROR") == 0)) {
                    ptr = exec_start;
                    interpret_line_at_ptr(&ptr, 0);
                    continue;
                }

                int index = (int)evaluate_expression(&ptr);
                Token jump_type = get_next_token(&ptr); // GOTO or GOSUB
                int count = 1;
                while (count < index) {
                    get_next_token(&ptr); // skip number
                    Token sep = get_next_token(&ptr);
                    if (sep.type != TOKEN_COMMA) { count = -1; break; }
                    count++;
                }
                if (count == index) {
                    Token target = get_next_token(&ptr);
                    if (jump_type.type == TOKEN_GOSUB) {
                        if (gosub_ptr < 32) {
                            gosub_call_stack[gosub_ptr].stmt = exec_stmt;
                            gosub_call_stack[gosub_ptr].ptr = ptr; // continue after the list
                            gosub_ptr++;
                            curr = find_line(target.int_val);
                            jumped = 1;
                            break;
                        }
                    } else {
                        curr = find_line(target.int_val);
                        jumped = 1;
                        break;
                    }
                } else {
                    // If index is out of bounds, BASICA continues to the next statement
                    while (*ptr && *ptr != ':') get_next_token(&ptr);
                }
                continue;
            }
            if (t.type == TOKEN_GOSUB) {
                Token target = get_next_token(&ptr);
                if (gosub_ptr < 32) {
                    gosub_call_stack[gosub_ptr].stmt = exec_stmt;
                    gosub_call_stack[gosub_ptr].ptr = ptr;
                    gosub_ptr++;
                    curr = find_line(target.int_val);
                    jumped = 1;
                    break;
                }
                else {
                    report_runtime_error(ERR_OUT_OF_MEMORY);
                    break;
                }
            }

            if (t.type == TOKEN_RETURN) {
                if (gosub_ptr > 0) {
                    gosub_ptr--;
                    curr = gosub_call_stack[gosub_ptr].stmt;
                    resume_ptr = gosub_call_stack[gosub_ptr].ptr;
                    jumped = 1;
                    break;
                } else {
                    report_runtime_error(ERR_RETURN_WITHOUT_GOSUB);
                    break;
                }
            }

            if (t.type == TOKEN_NEXT) {
                Token next_var = get_next_token(&ptr);
                int f = -1;
                if (next_var.type == TOKEN_IDENTIFIER) {
                    for (int i = for_ptr - 1; i >= 0; i--) {
                        if (strcasecmp(vars[for_stack[i].var_idx].name, next_var.text) == 0) {
                            f = i;
                            break;
                        }
                    }
                } else if (for_ptr > 0) {
                    f = for_ptr - 1;
                }
                if (f != -1) {
                    vars[for_stack[f].var_idx].value += for_stack[f].step_val;
                    double v = vars[for_stack[f].var_idx].value;
                    if ((for_stack[f].step_val > 0 && v <= for_stack[f].end_val) || (for_stack[f].step_val < 0 && v >= for_stack[f].end_val)) {
                        curr = for_stack[f].start_stmt;
                        resume_ptr = for_stack[f].start_ptr;
                        jumped = 1;
                        break;
                    } else {
                        for_ptr = f;
                    }
                } else {
                    report_runtime_error(ERR_NEXT_WITHOUT_FOR);
                }
                continue;
            }

            if (t.type == TOKEN_GOTO) {
                Token target = get_next_token(&ptr);
                Statement *target_stmt = find_line(target.int_val);
                if (!target_stmt) {
                    report_runtime_error(ERR_UNDEFINED_LINE_NUMBER);
                } else {
                    curr = target_stmt;
                    jumped = 1;
                }
                break;
            }

            if (t.type == TOKEN_END) {
                stop_running = 1;
                continue;
            }

            // Handle stray ELSE token (should not execute if we got here)
            if (t.type == TOKEN_ELSE) {
                // Skip this ELSE clause - it means condition was true or we just finished THEN part
                ptr = "";
                continue;
            }

            if (t.type == TOKEN_RESUME) {
                const char *saved = ptr;
                Token rt = get_next_token(&ptr);
                if (rt.type == TOKEN_NEXT) {
                    curr = error_stmt;
                    resume_ptr = error_next_ptr;
                } else if (rt.type == TOKEN_NUMBER) {
                    curr = find_line(rt.int_val);
                    resume_ptr = NULL;
                } else {
                    ptr = saved;
                    curr = error_stmt;
                    resume_ptr = error_ptr;
                }
                if (!curr) {
                    report_runtime_error(ERR_CANT_RESUME);
                } else {
                    jumped = 1;
                }
                break;
            }

            // Pass the pointer back to the start of this command (or the exec_start for same-line FOR body)
            ptr = exec_start;
            interpret_line_at_ptr(&ptr, 0);
        }

        if (runtime_error_occurred) {
            error_stmt = exec_stmt;
            error_ptr = stmt_start;
            error_next_ptr = ptr;
        }

        if (!stop_running && !jumped) {
            curr = find_next_statement(current_executing_line);
        }

        if (runtime_error_occurred && on_error_goto_line > 0) {
            Statement *trap = find_line(on_error_goto_line);
            if (trap) {
                curr = trap;
                resume_ptr = NULL;
                runtime_error_occurred = 0;
                stop_running = 0;
                last_runtime_error_msg[0] = '\0';
                continue;
            }
        }
    }
}
