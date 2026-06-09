#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "program.h"
#include "interpreter.h" // For basic_output

static Statement *head = NULL;
static Statement **line_index = NULL;
static int line_index_count = 0;
static int index_dirty = 1;

static void update_line_index() {
    if (!index_dirty) return;
    if (line_index) {
        free(line_index);
        line_index = NULL;
    }
    line_index_count = 0;
    Statement *curr = head;
    while (curr) {
        line_index_count++;
        curr = curr->next;
    }
    if (line_index_count > 0) {
        line_index = malloc(sizeof(Statement*) * line_index_count);
        if (line_index) {
            curr = head;
            for (int i = 0; i < line_index_count; i++) {
                line_index[i] = curr;
                curr = curr->next;
            }
            index_dirty = 0;
        }
    } else {
        index_dirty = 0;
    }
}

void add_line(int line_num, const char *text) {
    index_dirty = 1;
    Statement **curr = &head;
    while (*curr && (*curr)->line_number < line_num) {
        curr = &((*curr)->next);
    }

    if (*curr && (*curr)->line_number == line_num) {
        strncpy((*curr)->raw_command, text, 255);
    } else {
        Statement *new_stmt = malloc(sizeof(Statement));
        new_stmt->line_number = line_num;
        strncpy(new_stmt->raw_command, text, 255);
        new_stmt->next = *curr;
        *curr = new_stmt;
    }
}

Statement* find_line(int line_num) {
    update_line_index();
    if (line_index && line_index_count > 0) {
        int low = 0, high = line_index_count - 1;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            if (line_index[mid]->line_number == line_num) return line_index[mid];
            if (line_index[mid]->line_number < line_num) low = mid + 1;
            else high = mid - 1;
        }
        return NULL;
    }
    Statement *curr = head;
    while (curr) {
        if (curr->line_number == line_num) return curr;
        curr = curr->next;
    }
    return NULL;
}

void list_program() {
    Statement *curr = head;
    while (curr) {
        char out_buf[256 + 16]; // Enough for line number + command + newline
        sprintf(out_buf, "%d %s\n", curr->line_number, curr->raw_command);
        basic_output(out_buf);
        curr = curr->next;
    }
}

void clear_program() {
    index_dirty = 1;
    Statement *curr = head;
    while (curr) {
        Statement *next = curr->next;
        free(curr);
        curr = next;
    }
    head = NULL;
}

void delete_line(int line_num) {
    index_dirty = 1;
    Statement **curr = &head;
    while (*curr) {
        if ((*curr)->line_number == line_num) {
            Statement *to_remove = *curr;
            *curr = to_remove->next;
            free(to_remove);
            return;
        }
        curr = &((*curr)->next);
    }
}

Statement* get_head() { return head; }
