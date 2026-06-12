#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "common.h"
#include "interpreter.h"
#include "graphics.h"
#include "program.h"

static int quiet_mode = 0;
static int exit_on_finish = 0;

static void print_usage(const char *prog) {
    printf("Usage: %s [options] [program.bas]\n", prog);
    printf("Options:\n");
    printf("  -w, --window     enable graphics window\n");
    printf("  -q, --quiet      suppress startup header and REPL prompts\n");
    printf("  -x, --exit-on-finish exit immediately after program finishes (only with -w)\n");
    printf("  -v, --version    show version information\n");
    printf("  -h, --help       show this help message\n");
}

void handle_sigint(int sig) {
    (void)sig;
    stop_running = 1;
}

void repl() {
    char buffer[256];
    int active = graphics_is_active();
    char header[128];
    snprintf(header, sizeof(header), "IBM BASICA Clone (DOS 2.1) v%s\nREADY.\n", BASICA_VERSION);
    if (!quiet_mode) {
        if (active) graphics_print(header);
        else printf("%s", header);
    }

    while (1) {
        stop_running = 0; // Reset flag for new input
        if (graphics_is_active()) {
            if (!quiet_mode) graphics_print("Ok\n> ");
            graphics_readline(buffer, sizeof(buffer));
        } else {
            handle_events();
            if (!quiet_mode) printf("Ok\n> ");
            if (!fgets(buffer, sizeof(buffer), stdin)) break;
        }
        
        if (stop_running) continue;

        buffer[strcspn(buffer, "\n")] = 0;
        interpret_line(buffer, 1);
    }
}

void run_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Error opening file");
        return;
    }
    char buffer[256];
    int first_line = 1;
    while (fgets(buffer, sizeof(buffer), f)) {
        buffer[strcspn(buffer, "\n")] = 0;
        // Skip shebang line so .bas files can be marked executable
        if (first_line) {
            first_line = 0;
            if (buffer[0] == '#' && buffer[1] == '!') continue;
        }
        interpret_line(buffer, 1);
    }
    fclose(f);
    run_program();

    if (graphics_is_active()) {
        if (!exit_on_finish) set_window_title("BASICA Virtual Framebuffer - Press any key to quit...");
        graphics_present_now();
        if (!exit_on_finish) wait_for_keypress();
    }
}

int main(int argc, char **argv) {
    int use_window = 0;
    const char *filename = NULL;

    // Handle Ctrl+C in terminal
    signal(SIGINT, handle_sigint);
    srand(time(NULL));

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--window") == 0) {
            use_window = 1;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            quiet_mode = 1;
        } else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--exit-on-finish") == 0) {
            exit_on_finish = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("BASICA %s\n", BASICA_VERSION);
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (filename == NULL) {
            filename = argv[i];
            extern void set_args(int argc, char **argv);
            set_args(argc - i, &argv[i]);
            break; // Remaining arguments are passed to the script
        }
    }

    if (use_window) {
        if (!init_graphics()) {
            fprintf(stderr, "Failed to initialize graphics window\n");
            return 1;
        }
    }

    if (filename) {
        run_file(filename);
    } else {
        repl();
    }
    return 0;
}
