/*
 * ITL (Incredibly Tiny Language) Interpreter
 * A VTL-2 inspired language interpreter with unique features
 * Compiled for Windows 11 x86_64 with GCC + PDCurses
 * Advanced REPL mode with line numbers and jumps
 *
 * Build (MinGW/MSYS2):
 *   gcc -O3 -I. -o itl.exe itl_interpreter.c pdcurses.a -lm -lgdi32 -luser32
 *
 * Changes from previous version:
 *  - Replaced conio.h I/O with PDCurses (screen, color, cursor control)
 *  - Added underscore '_' as variable (index 26, alongside A-Z)
 *  - Added screen functions: gotoxy, putch, getch, setfore, setback,
 *    setattr, getw, geth, clear
 *  - All terminal I/O routed through PDCurses (printw / wgetnstr / etc.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <curses.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MAX_LINE_LENGTH 4096
#define MAX_LINES 100000
#define MAX_ARRAY_SIZE 1000000
#define MAX_STRING_LENGTH 4096
#define MAX_FUNC_ARGS 8

/* Number of single-letter variables: A-Z plus '_' */
#define NUM_VARS 27

/* --------------------------------------------------------------------------
 * Convenience macros for the variable name ↔ index mapping.
 *   IS_VARNAME(c)  true if c is a valid single-letter variable name
 *   VARIDX(c)      maps variable character to 0-based index (A→0 … Z→25, _→26)
 *   VARCHAR(i)     maps 0-based index back to the variable character
 * -------------------------------------------------------------------------- */
#define IS_VARNAME(c)  (isupper((unsigned char)(c)) || (c) == '_')
#define VARIDX(c)      (isupper((unsigned char)(c)) ? ((c) - 'A') : 26)
#define VARCHAR(i)     ((i) < 26 ? ('A' + (i)) : '_')

/* Value types */
typedef enum {
    TYPE_UNDEFINED,
    TYPE_NUMBER,
    TYPE_STRING
} ValueType;

/* Value structure */
typedef struct {
    ValueType type;
    union {
        double num;
        char *str;
    } data;
} Value;

/* Global variables */
Value variables[NUM_VARS];     /* A-Z plus '_' (index 26) */
char **source_lines;           /* Source code lines */
int line_count;                /* Total number of lines */
int current_line;              /* Current executing line (1-based) */
double *array_data;            /* Dynamic array */
int array_size;                /* Current array size */
int in_forward_ref = 0;        /* Flag to prevent infinite recursion */
int repl_mode = 0;             /* Flag for REPL mode */
int show_assignments = 0;      /* Flag to show assignment results */
int need_newline = 0;          /* Track if output ended without '\n' */

/* REPL command history */
#define REPL_HISTORY_MAX 500
char *repl_history[REPL_HISTORY_MAX];
int   repl_history_count = 0;

volatile int g_interrupted = 0;
/* GDI graphics state */
static HWND   g_hwnd      = NULL;
static HDC    g_hdc_buf   = NULL;
static HBITMAP g_hbmp     = NULL;
static int    g_gfx_w     = 640;
static int    g_gfx_h     = 480;
static HPEN   g_pen       = NULL;
static HBRUSH g_brush     = NULL;
static COLORREF g_pen_color   = RGB(255,255,255);
static COLORREF g_brush_color = RGB(0,0,0);
static HANDLE g_gfx_thread = NULL;
static CRITICAL_SECTION g_gfx_cs;
/* Mouse state */
static volatile int g_mouse_x     = 0;
static volatile int g_mouse_y     = 0;
static volatile int g_mouse_btn   = 0;   /* bit mask pulsanti correnti */
static volatile int g_mouse_click = 0;   /* ultimo click (1/2/3), 0=consumato */
static volatile int g_mouse_drag  = 0;   /* bit mask durante movimento */
/* Text window mouse state */
static volatile int g_tmouse_x     = 0;
static volatile int g_tmouse_y     = 0;
static volatile int g_tmouse_click = 0;
static volatile int g_tmouse_drag  = 0;  /* bit mask: 1=left, 2=right, 4=middle */
/* Timing state */
static LARGE_INTEGER g_timer_freq    = {0};
static LARGE_INTEGER g_timer_start   = {0};
static LARGE_INTEGER g_timer_elapsed = {0};

BOOL WINAPI ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
        g_interrupted = 1;
        return TRUE;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------
 * PDCurses color/attribute state.
 * Colors 0-7 match PDCurses COLOR_BLACK … COLOR_WHITE.
 * Color pairs are pre-initialised as: pair = bg*8 + fg + 1
 * -------------------------------------------------------------------------- */
static int itl_fg   = COLOR_WHITE;  /* current foreground color (0-7) */
static int itl_bg   = COLOR_BLACK;  /* current background color (0-7) */
static int itl_attr = A_NORMAL;     /* current character attribute    */

/* Expression parsing context */
typedef struct {
    const char *expr;
    int pos;
    int line_num;
} ParseContext;

/* Forward declarations */
void init_interpreter(void);
void cleanup_interpreter(void);
int load_source(const char *filename);
void execute_program(void);
void execute_from_line(int start_line);
Value evaluate_expression(ParseContext *ctx);
Value parse_primary(ParseContext *ctx);
void execute_line(int line_num);
int  execute_repl_command(const char *cmd);
void set_variable(int var_index, Value val);
Value get_variable(int var_index);
void free_value(Value *val);
Value copy_value(Value val);
void error(int line_num, const char *line, const char *message);
char *value_to_string(Value val);
double value_to_number(Value val);
void skip_whitespace(ParseContext *ctx);
int is_operator_char(char c);
void print_escaped_string(const char *str);
void run_repl(void);
void print_repl_help(void);
void print_repl_syntax_help(void);
void print_repl_screen_help(void);
void add_repl_line(const char *line);
Value call_math_function(const char *name, double *args, int nargs);
Value call_screen_function(const char *name, Value *args, int nargs);
static void gfx_open(int w, int h);
static void gfx_refresh(void);

/* ------------------------------------------------------------------ */
/* PDCurses helpers                                                     */
/* ------------------------------------------------------------------ */

/* Apply the current ITL fg/bg/attr to stdscr so subsequent addch/printw
   use the right color pair. */
static void apply_attrs(void) {
    int pair = itl_bg * 8 + itl_fg + 1;
    wattrset(stdscr, itl_attr | COLOR_PAIR(pair));
}

/* Initialise all 64 fg×bg color pairs.
   Pair numbering: pair = bg*8 + fg + 1  (pairs start at 1) */
static void init_color_pairs(void) {
    int bg, fg;
    for (bg = 0; bg < 8; bg++)
        for (fg = 0; fg < 8; fg++)
            init_pair((short)(bg * 8 + fg + 1), (short)fg, (short)bg);
}

LRESULT CALLBACK GfxWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        EnterCriticalSection(&g_gfx_cs);
        BitBlt(hdc, 0,0, g_gfx_w, g_gfx_h, g_hdc_buf, 0,0, SRCCOPY);
        LeaveCriticalSection(&g_gfx_cs);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_DESTROY) { g_hwnd = NULL; return 0; }
    if (msg == WM_SETCURSOR) {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }
    if (msg == WM_MOUSEMOVE) {
        g_mouse_x    = LOWORD(lp);
        g_mouse_y    = HIWORD(lp);
        g_mouse_drag = 0;
        if (wp & MK_LBUTTON) g_mouse_drag |= 1;
        if (wp & MK_RBUTTON) g_mouse_drag |= 2;
        if (wp & MK_MBUTTON) g_mouse_drag |= 4;
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        g_mouse_x   = LOWORD(lp);
        g_mouse_y   = HIWORD(lp);
        g_mouse_btn |= 1;
        g_mouse_click = 1;
        return 0;
    }
    if (msg == WM_RBUTTONDOWN) {
        g_mouse_x   = LOWORD(lp);
        g_mouse_y   = HIWORD(lp);
        g_mouse_btn |= 2;
        g_mouse_click = 2;
        return 0;
    }
    if (msg == WM_MBUTTONDOWN) {
        g_mouse_x   = LOWORD(lp);
        g_mouse_y   = HIWORD(lp);
        g_mouse_btn |= 4;
        g_mouse_click = 3;
        return 0;
    }
    if (msg == WM_LBUTTONUP) { g_mouse_btn &= ~1; g_mouse_drag &= ~1; return 0; }
    if (msg == WM_RBUTTONUP) { g_mouse_btn &= ~2; g_mouse_drag &= ~2; return 0; }
    if (msg == WM_MBUTTONUP) { g_mouse_btn &= ~4; g_mouse_drag &= ~4; return 0; }
    return DefWindowProc(hwnd, msg, wp, lp);
}

DWORD WINAPI gfx_thread_func(LPVOID param) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = GfxWndProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpszClassName = "ITLGfx";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    g_hwnd = CreateWindow("ITLGfx", "ITL Graphics",
        WS_OVERLAPPEDWINDOW, 100, 100,
        g_gfx_w + 16, g_gfx_h + 39,
        NULL, NULL, wc.hInstance, NULL);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

static void gfx_open(int w, int h) {
    if (g_hwnd) return;  /* già aperta */
    g_gfx_w = w; g_gfx_h = h;
    InitializeCriticalSection(&g_gfx_cs);

    /* Crea backbuffer */
    HDC hdc_screen = GetDC(NULL);
    g_hdc_buf = CreateCompatibleDC(hdc_screen);
    g_hbmp    = CreateCompatibleBitmap(hdc_screen, w, h);
    SelectObject(g_hdc_buf, g_hbmp);
    ReleaseDC(NULL, hdc_screen);

    /* Riempie di nero */
    RECT r = {0,0,w,h};
    FillRect(g_hdc_buf, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));

    /* Penna e pennello di default */
    g_pen   = CreatePen(PS_SOLID, 1, g_pen_color);
    g_brush = CreateSolidBrush(g_brush_color);
    SelectObject(g_hdc_buf, g_pen);
    SelectObject(g_hdc_buf, g_brush);

    g_gfx_thread = CreateThread(NULL, 0, gfx_thread_func, NULL, 0, NULL);
    Sleep(100);  /* attende che la finestra sia pronta */
}

static void gfx_refresh(void) {
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

/* ------------------------------------------------------------------ */
/* Initialize interpreter state                                         */
/* ------------------------------------------------------------------ */
void init_interpreter(void) {
    int i;

    for (i = 0; i < NUM_VARS; i++)
        variables[i].type = TYPE_UNDEFINED;

    array_size = 0;
    array_data = NULL;

    /* Seed RNG from current time so each run produces a new sequence */
    srand((unsigned int)time(NULL));

    source_lines = NULL;
    line_count = 0;
    current_line = 0;

    QueryPerformanceFrequency(&g_timer_freq);
    QueryPerformanceCounter(&g_timer_start);
    g_timer_elapsed = g_timer_start;
}

/* ------------------------------------------------------------------ */
/* Cleanup interpreter resources                                        */
/* ------------------------------------------------------------------ */
void cleanup_interpreter(void) {
    int i;

    for (i = 0; i < NUM_VARS; i++) {
        if (variables[i].type == TYPE_STRING && variables[i].data.str)
            free(variables[i].data.str);
    }

    if (source_lines) {
        for (i = 0; i < line_count; i++)
            free(source_lines[i]);
        free(source_lines);
    }

    if (array_data)
        free(array_data);

    if (g_hwnd) SendMessage(g_hwnd, WM_DESTROY, 0, 0);
    if (g_gfx_thread) { WaitForSingleObject(g_gfx_thread, 1000); CloseHandle(g_gfx_thread); }
    if (g_hdc_buf) DeleteDC(g_hdc_buf);
    if (g_hbmp)    DeleteObject(g_hbmp);
    if (g_pen)     DeleteObject(g_pen);
    if (g_brush)   DeleteObject(g_brush);
    if (g_hwnd)    DeleteCriticalSection(&g_gfx_cs);

}

/* ------------------------------------------------------------------ */
/* Value helpers                                                        */
/* ------------------------------------------------------------------ */
void free_value(Value *val) {
    if (val->type == TYPE_STRING && val->data.str) {
        free(val->data.str);
        val->data.str = NULL;
    }
    val->type = TYPE_UNDEFINED;
}

Value copy_value(Value val) {
    Value result;
    result.type = val.type;
    if (val.type == TYPE_STRING)
        result.data.str = _strdup(val.data.str);
    else
        result.data = val.data;
    return result;
}

char *value_to_string(Value val) {
    char *result = (char *)malloc(MAX_STRING_LENGTH);
    if (val.type == TYPE_NUMBER) {
        snprintf(result, MAX_STRING_LENGTH, "%.15g", val.data.num);
    } else if (val.type == TYPE_STRING) {
        strncpy(result, val.data.str, MAX_STRING_LENGTH - 1);
        result[MAX_STRING_LENGTH - 1] = '\0';
    } else {
        strcpy(result, "0");
    }
    return result;
}

double value_to_number(Value val) {
    if (val.type == TYPE_NUMBER)
        return val.data.num;
    if (val.type == TYPE_STRING) {
        char *endptr;
        double num = strtod(val.data.str, &endptr);
        if (endptr == val.data.str) return 0.0;
        return num;
    }
    return 0.0;
}

/* ------------------------------------------------------------------ */
/* Error reporting                                                      */
/* ------------------------------------------------------------------ */
void error(int line_num, const char *line, const char *message) {
    printw("Error at line %d: %s\n", line_num, message);
    if (line)
        printw("Line content: %s\n", line);
    refresh();
    if (!repl_mode) {
        endwin();
        cleanup_interpreter();
        exit(1);
    }
}

/* ------------------------------------------------------------------ */
/* Print string with escape sequences (PDCurses version)               */
/* ------------------------------------------------------------------ */
void print_escaped_string(const char *str) {
    int i = 0;
    apply_attrs();
    while (str[i]) {
        if (str[i] == '\\' && str[i + 1]) {
            if (str[i + 1] >= '0' && str[i + 1] <= '7') {
                int octal_value = 0;
                int j = i + 1;
                for (int count = 0; count < 3 && str[j] >= '0' && str[j] <= '7'; count++) {
                    octal_value = octal_value * 8 + (str[j] - '0');
                    j++;
                }
                addch((unsigned char)octal_value);
                i = j;
            } else if (str[i + 1] == 'n') { addch('\n'); i += 2; }
            else if (str[i + 1] == 't') { addch('\t'); i += 2; }
            else if (str[i + 1] == 'r') { addch('\r'); i += 2; }
            else if (str[i + 1] == '\\') { addch('\\'); i += 2; }
            else if (str[i + 1] == '"') { addch('"'); i += 2; }
            else { addch((unsigned char)str[i + 1]); i += 2; }
        } else {
            addch((unsigned char)str[i]);
            i++;
        }
    }
    refresh();
}

/* ------------------------------------------------------------------ */
/* Paren-aware semicolon split helper                                  */
/* Splits 'input' on ';' characters that are NOT inside parentheses   */
/* or double-quoted strings. Appends each segment to source_lines[].  */
/* ------------------------------------------------------------------ */
static void split_and_store(const char *input, char ***lines, int *count, int *capacity) {
    const char *p = input;
    char seg[MAX_LINE_LENGTH];
    int si = 0;
    int depth = 0;   /* paren nesting depth */
    int in_str = 0;  /* inside a double-quoted string */

    while (*p) {
        char c = *p;

        if (in_str) {
            /* Inside a string: only watch for closing quote and backslash escapes */
            if (c == '\\' && *(p + 1)) {
                if (si < MAX_LINE_LENGTH - 1) seg[si++] = c;
                p++;
                c = *p;
                if (si < MAX_LINE_LENGTH - 1) seg[si++] = c;
                p++;
                continue;
            }
            if (c == '"') in_str = 0;
            if (si < MAX_LINE_LENGTH - 1) seg[si++] = c;
            p++;
            continue;
        }

        if (c == '"') {
            in_str = 1;
            if (si < MAX_LINE_LENGTH - 1) seg[si++] = c;
            p++;
            continue;
        }

        if (c == '(') depth++;
        if (c == ')') { if (depth > 0) depth--; }

        /* Split only on top-level semicolons (depth == 0) */
        if (c == ';' && depth == 0) {
            seg[si] = '\0';
            if (*count >= *capacity) {
                *capacity *= 2;
                *lines = (char **)realloc(*lines, *capacity * sizeof(char *));
            }
            (*lines)[(*count)++] = _strdup(seg);
            si = 0;
            p++;
            continue;
        }

        if (si < MAX_LINE_LENGTH - 1) seg[si++] = c;
        p++;
    }

    /* Store the last (or only) segment */
    seg[si] = '\0';
    if (*count >= *capacity) {
        *capacity *= 2;
        *lines = (char **)realloc(*lines, *capacity * sizeof(char *));
    }
    (*lines)[(*count)++] = _strdup(seg);
}

/* ------------------------------------------------------------------ */
/* Load source file (splits on top-level semicolons only)             */
/* ------------------------------------------------------------------ */
int load_source(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char buffer[MAX_LINE_LENGTH];
    int capacity = 1000;
    source_lines = (char **)malloc(capacity * sizeof(char *));
    line_count = 0;

    while (fgets(buffer, sizeof(buffer), fp)) {
        int len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
            buffer[--len] = '\0';

        split_and_store(buffer, &source_lines, &line_count, &capacity);
    }

    fclose(fp);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Add a line to the REPL program (splitting on top-level semicolons) */
/* ------------------------------------------------------------------ */
void add_repl_line(const char *line) {
    static int capacity = 0;

    if (source_lines == NULL) {
        capacity = 1000;
        source_lines = (char **)malloc(capacity * sizeof(char *));
        line_count = 0;
    }

    split_and_store(line, &source_lines, &line_count, &capacity);
}

/* ------------------------------------------------------------------ */
/* Helper: skip whitespace                                             */
/* ------------------------------------------------------------------ */
void skip_whitespace(ParseContext *ctx) {
    while (ctx->expr[ctx->pos] == ' ' || ctx->expr[ctx->pos] == '\t')
        ctx->pos++;
}

int is_operator_char(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
           c == '^' || c == '&' || c == '|' || c == '<' || c == '>' ||
           c == '=' || c == '!';
}

/* ------------------------------------------------------------------ */
/* Variable accessors (A-Z at indices 0-25, '_' at index 26)          */
/* ------------------------------------------------------------------ */
void set_variable(int var_index, Value val) {
    if (var_index < 0 || var_index >= NUM_VARS) return;

    if (variables[var_index].type == TYPE_STRING && variables[var_index].data.str)
        free(variables[var_index].data.str);

    variables[var_index] = copy_value(val);

    if (repl_mode && show_assignments) {
        printw("< %c = ", (char)VARCHAR(var_index));
        if (val.type == TYPE_NUMBER)
            printw("%.15g\n", val.data.num);
        else if (val.type == TYPE_STRING)
            printw("\"%s\"\n", val.data.str);
        else
            printw("undefined\n");
        refresh();
    }
}

Value get_variable(int var_index) {
    if (var_index < 0 || var_index >= NUM_VARS) {
        Value undef; undef.type = TYPE_UNDEFINED; return undef;
    }

    if (variables[var_index].type == TYPE_UNDEFINED && !in_forward_ref) {
        char var_name = (char)VARCHAR(var_index);
        int saved_line = current_line;
        in_forward_ref = 1;

        for (int i = current_line; i < line_count; i++) {
            const char *line = source_lines[i];
            int pos = 0;
            while (line[pos] == ' ' || line[pos] == '\t') pos++;
            if (line[pos] == var_name && line[pos + 1] != '\0') {
                execute_line(i + 1);
                break;
            }
        }

        in_forward_ref = 0;
        current_line = saved_line;
    }

    return copy_value(variables[var_index]);
}

/* ------------------------------------------------------------------ */
/* Math function dispatcher                                            */
/* ------------------------------------------------------------------ */
Value call_math_function(const char *name, double *args, int nargs) {
    Value result;
    result.type = TYPE_NUMBER;
    result.data.num = 0.0;

    /* 1-argument functions */
    if      (strcmp(name, "sin")   == 0 && nargs >= 1) { result.data.num = sin(args[0]); }
    else if (strcmp(name, "cos")   == 0 && nargs >= 1) { result.data.num = cos(args[0]); }
    else if (strcmp(name, "tan")   == 0 && nargs >= 1) { result.data.num = tan(args[0]); }
    else if (strcmp(name, "asin")  == 0 && nargs >= 1) { result.data.num = asin(args[0]); }
    else if (strcmp(name, "acos")  == 0 && nargs >= 1) { result.data.num = acos(args[0]); }
    else if (strcmp(name, "atan")  == 0 && nargs >= 1) { result.data.num = atan(args[0]); }
    else if (strcmp(name, "sinh")  == 0 && nargs >= 1) { result.data.num = sinh(args[0]); }
    else if (strcmp(name, "cosh")  == 0 && nargs >= 1) { result.data.num = cosh(args[0]); }
    else if (strcmp(name, "tanh")  == 0 && nargs >= 1) { result.data.num = tanh(args[0]); }
    else if (strcmp(name, "exp")   == 0 && nargs >= 1) { result.data.num = exp(args[0]); }
    else if (strcmp(name, "log")   == 0 && nargs >= 1) { result.data.num = log(args[0]); }
    else if (strcmp(name, "log2")  == 0 && nargs >= 1) { result.data.num = log2(args[0]); }
    else if (strcmp(name, "log10") == 0 && nargs >= 1) { result.data.num = log10(args[0]); }
    else if (strcmp(name, "sqrt")  == 0 && nargs >= 1) { result.data.num = sqrt(args[0]); }
    else if (strcmp(name, "cbrt")  == 0 && nargs >= 1) { result.data.num = cbrt(args[0]); }
    else if (strcmp(name, "ceil")  == 0 && nargs >= 1) { result.data.num = ceil(args[0]); }
    else if (strcmp(name, "floor") == 0 && nargs >= 1) { result.data.num = floor(args[0]); }
    else if (strcmp(name, "round") == 0 && nargs >= 1) { result.data.num = round(args[0]); }
    else if (strcmp(name, "trunc") == 0 && nargs >= 1) { result.data.num = trunc(args[0]); }
    else if (strcmp(name, "fabs") == 0 && nargs >= 1)  { result.data.num = fabs(args[0]); }
    else if (strcmp(name, "abs")   == 0 && nargs >= 1) { result.data.num = fabs(args[0]); }
    else if (strcmp(name, "sign")  == 0 && nargs >= 1) {
        result.data.num = (args[0] > 0) ? 1.0 : (args[0] < 0) ? -1.0 : 0.0;
    }
    /* 2-argument functions */
    else if (strcmp(name, "atan2") == 0 && nargs >= 2) { result.data.num = atan2(args[0], args[1]); }
    else if (strcmp(name, "pow")   == 0 && nargs >= 2) { result.data.num = pow(args[0], args[1]); }
    else if (strcmp(name, "fmod")  == 0 && nargs >= 2) { result.data.num = fmod(args[0], args[1]); }
    else if (strcmp(name, "hypot") == 0 && nargs >= 2) { result.data.num = hypot(args[0], args[1]); }
    else if (strcmp(name, "fmax")  == 0 && nargs >= 2) { result.data.num = fmax(args[0], args[1]); }
    else if (strcmp(name, "fmin")  == 0 && nargs >= 2) { result.data.num = fmin(args[0], args[1]); }
    else if (strcmp(name, "max")   == 0 && nargs >= 2) { result.data.num = fmax(args[0], args[1]); }
    else if (strcmp(name, "min")   == 0 && nargs >= 2) { result.data.num = fmin(args[0], args[1]); }
    /* Constants as zero-arg "functions" */
    else if (strcmp(name, "pi")    == 0) { result.data.num = M_PI; }
    else if (strcmp(name, "e")     == 0) { result.data.num = M_E; }
    else {
        printw("Warning: unknown function '%s'\n", name);
        refresh();
        result.type = TYPE_UNDEFINED;
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* Screen function dispatcher (uses PDCurses API)                      */
/*                                                                      */
/* Functions:                                                           */
/*   gotoxy(x,y)       - move cursor; returns 1 on success, 0 on fail  */
/*   putch(c)          - write char/string at cursor; returns old ASCII */
/*   getch()           - read char code at cursor; returns ASCII or -1  */
/*   setfore(color)    - set foreground color 0-7; returns 1/0          */
/*   setback(color)    - set background color 0-7; returns 1/0          */
/*   setattr(attr)     - set attribute 0=normal,1=bold,2=reverse        */
/*   getw()            - return number of visible columns               */
/*   geth()            - return number of visible rows                  */
/*   clear()           - clear screen with current background color     */
/* ------------------------------------------------------------------ */
Value call_screen_function(const char *name, Value *args, int nargs) {
    Value result;
    result.type  = TYPE_NUMBER;
    result.data.num = 0.0;

    /* gotoxy(x, y) -------------------------------------------------- */
    if (strcmp(name, "gotoxy") == 0) {
        if (nargs >= 2) {
            int x    = (int)value_to_number(args[0]);
            int y    = (int)value_to_number(args[1]);
            int rows = 0, cols = 0;
            getmaxyx(stdscr, rows, cols);
            if (x >= 0 && x < cols && y >= 0 && y < rows) {
                move(y, x);
                refresh();
                result.data.num = 1.0;
            }
            /* else result stays 0 (failure) */
        }
        return result;
    }

    /* putch(c) ------------------------------------------------------- */
    if (strcmp(name, "putch") == 0) {
        if (nargs >= 1) {
            /* Capture the character currently at the cursor position */
            chtype old_ch = inch();
            result.data.num = (double)(old_ch & 0xFF);

            apply_attrs();
            if (args[0].type == TYPE_STRING) {
                /* String argument: print the whole string */
                const char *p = args[0].data.str;
                while (*p) addch((unsigned char)*p++);
            } else {
                /* Numeric argument: treat as ASCII code */
                int ch = (int)value_to_number(args[0]);
                if (ch >= 0 && ch <= 255) {
                    addch((unsigned char)ch);
                } else {
                    result.data.num = -1.0;
                }
            }
            refresh();
        } else {
            result.data.num = -1.0;
        }
        return result;
    }

    /* getch() -------------------------------------------------------- */
    /* Reads the character at the current cursor position (screen read) */
    if (strcmp(name, "getch") == 0) {
        chtype ch = inch();
        result.data.num = (ch == (chtype)ERR) ? -1.0 : (double)(ch & 0xFF);
        return result;
    }

    /* setfore(color) ------------------------------------------------- */
    if (strcmp(name, "setfore") == 0) {
        if (nargs >= 1) {
            int color = (int)value_to_number(args[0]);
            if (color >= 0 && color <= 7) {
                itl_fg = color;
                apply_attrs();
                result.data.num = 1.0;
            }
        }
        return result;
    }

    /* setback(color) ------------------------------------------------- */
    if (strcmp(name, "setback") == 0) {
        if (nargs >= 1) {
            int color = (int)value_to_number(args[0]);
            if (color >= 0 && color <= 7) {
                itl_bg = color;
                apply_attrs();
                result.data.num = 1.0;
            }
        }
        return result;
    }

    /* setattr(attr) -------------------------------------------------- */
    /* 0=normal, 1=bold, 2=reverse                                       */
    if (strcmp(name, "setattr") == 0) {
        if (nargs >= 1) {
            int attr = (int)value_to_number(args[0]);
            switch (attr) {
                case 1:  itl_attr = A_BOLD;    break;
                case 2:  itl_attr = A_REVERSE; break;
                default: itl_attr = A_NORMAL;  break;
            }
            apply_attrs();
            result.data.num = (double)attr;
        }
        return result;
    }

    /* getw() --------------------------------------------------------- */
    if (strcmp(name, "getw") == 0) {
        int rows = 0, cols = 0;
        getmaxyx(stdscr, rows, cols);
        (void)rows;
        result.data.num = (double)cols;
        return result;
    }

    /* geth() --------------------------------------------------------- */
    if (strcmp(name, "geth") == 0) {
        int rows = 0, cols = 0;
        getmaxyx(stdscr, rows, cols);
        (void)cols;
        result.data.num = (double)rows;
        return result;
    }

    /* clear() -------------------------------------------------------- */
    /* Fills screen with current background color and moves cursor to    */
    /* the top-left corner.                                              */
    if (strcmp(name, "clear") == 0) {
        int pair = itl_bg * 8 + itl_fg + 1;
        /* bkgd sets the default fill character + color for the window  */
        bkgd((chtype)(COLOR_PAIR(pair) | ' '));
        /* PDCurses clear() erases all cells and schedules full repaint  */
        wclear(stdscr);
        move(0, 0);
        refresh();
        result.data.num = 1.0;
        return result;
    }

    /* tmx() ---------------------------------------------------------- */
    if (strcmp(name, "tmx") == 0) {
        result.data.num = (double)g_tmouse_x;
        return result;
    }

    /* tmy() ---------------------------------------------------------- */
    if (strcmp(name, "tmy") == 0) {
        result.data.num = (double)g_tmouse_y;
        return result;
    }

    /* tmclick() ------------------------------------------------------ */
    /* Ritorna il pulsante dell'ultimo click (1/2/3) e lo azzera       */
    if (strcmp(name, "tmclick") == 0) {
        result.data.num = (double)g_tmouse_click;
        g_tmouse_click = 0;
        return result;
    }

    /* tmdrag(b) ------------------------------------------------------ */
    /* Ritorna 1 se il pulsante b (1/2/3) è tenuto premuto             */
    /* mentre il mouse si muove, 0 altrimenti                          */
    if (strcmp(name, "tmdrag") == 0) {
        int btn = (nargs >= 1) ? (int)value_to_number(args[0]) : 1;
        int bit = (btn == 1) ? 1 : (btn == 2) ? 2 : 4;
        result.data.num = (g_tmouse_drag & bit) ? 1.0 : 0.0;
        return result;
    }

    /* gopen(w,h) ---------------------------------------------------- */
    if (strcmp(name, "gopen") == 0) {
        int w = (nargs >= 1) ? (int)value_to_number(args[0]) : 640;
        int h = (nargs >= 2) ? (int)value_to_number(args[1]) : 480;
        gfx_open(w, h);
        result.data.num = 1.0;
        return result;
    }

    /* gclear() ------------------------------------------------------- */
    if (strcmp(name, "gclear") == 0) {
        if (g_hdc_buf) {
            EnterCriticalSection(&g_gfx_cs);
            RECT r = {0, 0, g_gfx_w, g_gfx_h};
            HBRUSH b = CreateSolidBrush(g_brush_color);
            FillRect(g_hdc_buf, &r, b);
            DeleteObject(b);
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* gpen(r,g,b) ---------------------------------------------------- */
    if (strcmp(name, "gpen") == 0) {
        if (nargs >= 3) {
            int r = (int)value_to_number(args[0]);
            int g = (int)value_to_number(args[1]);
            int b = (int)value_to_number(args[2]);
            g_pen_color = RGB(r, g, b);
            EnterCriticalSection(&g_gfx_cs);
            HPEN new_pen = CreatePen(PS_SOLID, 1, g_pen_color);
            SelectObject(g_hdc_buf, new_pen);
            if (g_pen) DeleteObject(g_pen);
            g_pen = new_pen;
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* gbr(r,g,b) ----------------------------------------------------- */
    if (strcmp(name, "gbr") == 0) {
        if (nargs >= 3) {
            int r = (int)value_to_number(args[0]);
            int g = (int)value_to_number(args[1]);
            int b = (int)value_to_number(args[2]);
            g_brush_color = RGB(r, g, b);
            EnterCriticalSection(&g_gfx_cs);
            HBRUSH new_brush = CreateSolidBrush(g_brush_color);
            SelectObject(g_hdc_buf, new_brush);
            if (g_brush) DeleteObject(g_brush);
            g_brush = new_brush;
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* gpixel(x,y) ---------------------------------------------------- */
    if (strcmp(name, "gpixel") == 0) {
        if (nargs >= 2 && g_hdc_buf) {
            int x = (int)value_to_number(args[0]);
            int y = (int)value_to_number(args[1]);
            EnterCriticalSection(&g_gfx_cs);
            SetPixel(g_hdc_buf, x, y, g_pen_color);
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* gline(x1,y1,x2,y2) -------------------------------------------- */
    if (strcmp(name, "gline") == 0) {
        if (nargs >= 4 && g_hdc_buf) {
            int x1 = (int)value_to_number(args[0]);
            int y1 = (int)value_to_number(args[1]);
            int x2 = (int)value_to_number(args[2]);
            int y2 = (int)value_to_number(args[3]);
            EnterCriticalSection(&g_gfx_cs);
            MoveToEx(g_hdc_buf, x1, y1, NULL);
            LineTo(g_hdc_buf, x2, y2);
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* grect(x1,y1,x2,y2) -------------------------------------------- */
    /* Disegna solo il bordo (usa penna corrente, pennello trasparente)  */
    if (strcmp(name, "grect") == 0) {
        if (nargs >= 4 && g_hdc_buf) {
            int x1 = (int)value_to_number(args[0]);
            int y1 = (int)value_to_number(args[1]);
            int x2 = (int)value_to_number(args[2]);
            int y2 = (int)value_to_number(args[3]);
            EnterCriticalSection(&g_gfx_cs);
            HBRUSH old_brush = (HBRUSH)SelectObject(g_hdc_buf,
                                   GetStockObject(NULL_BRUSH));
            Rectangle(g_hdc_buf, x1, y1, x2, y2);
            SelectObject(g_hdc_buf, old_brush);
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* gfillrect(x1,y1,x2,y2) ---------------------------------------- */
    /* Rettangolo pieno con pennello corrente                            */
    if (strcmp(name, "gfillrect") == 0) {
        if (nargs >= 4 && g_hdc_buf) {
            int x1 = (int)value_to_number(args[0]);
            int y1 = (int)value_to_number(args[1]);
            int x2 = (int)value_to_number(args[2]);
            int y2 = (int)value_to_number(args[3]);
            EnterCriticalSection(&g_gfx_cs);
            Rectangle(g_hdc_buf, x1, y1, x2, y2);
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* gcircle(x,y,r) ------------------------------------------------- */
    /* Ellisse (solo bordo) centrata in (x,y) con raggio r              */
    if (strcmp(name, "gcircle") == 0) {
        if (nargs >= 3 && g_hdc_buf) {
            int x = (int)value_to_number(args[0]);
            int y = (int)value_to_number(args[1]);
            int r = (int)value_to_number(args[2]);
            EnterCriticalSection(&g_gfx_cs);
            HBRUSH old_brush = (HBRUSH)SelectObject(g_hdc_buf,
                                   GetStockObject(NULL_BRUSH));
            Ellipse(g_hdc_buf, x - r, y - r, x + r, y + r);
            SelectObject(g_hdc_buf, old_brush);
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* gfillcircle(x,y,r) -------------------------------------------- */
    /* Ellisse piena con pennello corrente                               */
    if (strcmp(name, "gfillcircle") == 0) {
        if (nargs >= 3 && g_hdc_buf) {
            int x = (int)value_to_number(args[0]);
            int y = (int)value_to_number(args[1]);
            int r = (int)value_to_number(args[2]);
            EnterCriticalSection(&g_gfx_cs);
            Ellipse(g_hdc_buf, x - r, y - r, x + r, y + r);
            LeaveCriticalSection(&g_gfx_cs);
            result.data.num = 1.0;
        }
        return result;
    }

    /* gtext(x,y,str) ------------------------------------------------- */
    /* Scrive testo in posizione pixel (x,y) con colore penna corrente  */
    if (strcmp(name, "gtext") == 0) {
        if (nargs >= 3 && g_hdc_buf) {
            int x = (int)value_to_number(args[0]);
            int y = (int)value_to_number(args[1]);
            char *str = (args[2].type == TYPE_STRING)
                        ? args[2].data.str
                        : value_to_string(args[2]);
            EnterCriticalSection(&g_gfx_cs);
            SetTextColor(g_hdc_buf, g_pen_color);
            SetBkMode(g_hdc_buf, TRANSPARENT);
            TextOut(g_hdc_buf, x, y, str, (int)strlen(str));
            LeaveCriticalSection(&g_gfx_cs);
            if (args[2].type != TYPE_STRING) free(str);
            result.data.num = 1.0;
        }
        return result;
    }

    /* grefresh() ----------------------------------------------------- */
    if (strcmp(name, "grefresh") == 0) {
        gfx_refresh();
        result.data.num = 1.0;
        return result;
    }

    /* gmx() ---------------------------------------------------------- */
    if (strcmp(name, "gmx") == 0) {
        result.data.num = (double)g_mouse_x;
        return result;
    }

    /* gmy() ---------------------------------------------------------- */
    if (strcmp(name, "gmy") == 0) {
        result.data.num = (double)g_mouse_y;
        return result;
    }

    /* gmb() ---------------------------------------------------------- */
    /* Ritorna la maschera dei pulsanti attualmente premuti            */
    /* bit 0=sinistro, bit 1=destro, bit 2=centrale                   */
    if (strcmp(name, "gmb") == 0) {
        result.data.num = (double)g_mouse_btn;
        return result;
    }

    /* gmclick() ------------------------------------------------------ */
    /* Ritorna il pulsante dell'ultimo click (1/2/3) e lo azzera       */
    if (strcmp(name, "gmclick") == 0) {
        result.data.num = (double)g_mouse_click;
        g_mouse_click = 0;
        return result;
    }

    /* gmdrag(b) ------------------------------------------------------ */
    /* Ritorna 1 se il pulsante b (1/2/3) è tenuto premuto             */
    /* mentre il mouse si muove, 0 altrimenti                          */
    if (strcmp(name, "gmdrag") == 0) {
        int btn = (nargs >= 1) ? (int)value_to_number(args[0]) : 1;
        int bit = (btn == 1) ? 1 : (btn == 2) ? 2 : 4;
        result.data.num = (g_mouse_drag & bit) ? 1.0 : 0.0;
        return result;
    }

    /* time() -------------------------------------------------------- */
    /* Restituisce i secondi interi trascorsi dall'epoca Unix (1/1/1970) */
    if (strcmp(name, "time") == 0) {
        result.data.num = (double)time(NULL);
        return result;
    }

    /* ticks() ------------------------------------------------------- */
    /* Restituisce i millisecondi trascorsi dall'avvio dell'interprete   */
    if (strcmp(name, "ticks") == 0) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        result.data.num = (double)(now.QuadPart - g_timer_start.QuadPart)
                          * 1000.0 / (double)g_timer_freq.QuadPart;
        return result;
    }

    /* elapsed() ----------------------------------------------------- */
    /* Restituisce i millisecondi trascorsi dall'ultima chiamata         */
    /* a elapsed() (o dall'avvio se non era mai stata chiamata)         */
    if (strcmp(name, "elapsed") == 0) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        result.data.num = (double)(now.QuadPart - g_timer_elapsed.QuadPart)
                          * 1000.0 / (double)g_timer_freq.QuadPart;
        g_timer_elapsed = now;
        return result;
    }

    /* Unknown screen function */
    result.type = TYPE_UNDEFINED;
    return result;
}

/* ------------------------------------------------------------------ */
/* Returns 1 if 'name' is a known screen function, 0 otherwise        */
/* ------------------------------------------------------------------ */
static int is_screen_function(const char *name) {
    static const char *screen_funcs[] = {
        "gotoxy", "putch", "getch", "setfore", "setback",
        "setattr", "getw", "geth", "clear",
        "tmx", "tmy", "tmclick", "tmdrag",
        "gopen","gclear","gpen","gbr","gpixel","gline",
        "grect","gfillrect","gcircle","gfillcircle","gtext","grefresh",
        "gmx", "gmy", "gmb", "gmclick", "gmdrag",
        "time", "ticks", "elapsed",
        NULL
    };
    for (int i = 0; screen_funcs[i]; i++)
        if (strcmp(name, screen_funcs[i]) == 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Parse primary expression                                            */
/* ------------------------------------------------------------------ */
Value parse_primary(ParseContext *ctx) {
    Value result;
    result.type = TYPE_UNDEFINED;

    skip_whitespace(ctx);

    /* Unary minus */
    if (ctx->expr[ctx->pos] == '-' &&
        (isdigit((unsigned char)ctx->expr[ctx->pos + 1]) ||
         IS_VARNAME(ctx->expr[ctx->pos + 1]) ||
         ctx->expr[ctx->pos + 1] == '(' ||
         ctx->expr[ctx->pos + 1] == '@' ||
         ctx->expr[ctx->pos + 1] == '?' ||
         ctx->expr[ctx->pos + 1] == '\'' ||
         ctx->expr[ctx->pos + 1] == '#' ||
         ctx->expr[ctx->pos + 1] == '$')) {
        ctx->pos++;
        Value val = parse_primary(ctx);
        result.type = TYPE_NUMBER;
        result.data.num = -value_to_number(val);
        free_value(&val);
        return result;
    }

    /* Unary logical NOT */
    if (ctx->expr[ctx->pos] == '!') {
        ctx->pos++;
        Value val = parse_primary(ctx);
        result.type = TYPE_NUMBER;
        result.data.num = (value_to_number(val) == 0.0) ? 1.0 : 0.0;
        free_value(&val);
        return result;
    }

    /* Type conversion ($VAR) */
    if (ctx->expr[ctx->pos] == '$') {
        ctx->pos++;
        skip_whitespace(ctx);

        if (IS_VARNAME(ctx->expr[ctx->pos])) {
            int var_idx = VARIDX(ctx->expr[ctx->pos]);
            ctx->pos++;
            Value var_val = get_variable(var_idx);

            if (var_val.type == TYPE_NUMBER) {
                result.type = TYPE_STRING;
                result.data.str = value_to_string(var_val);
            } else if (var_val.type == TYPE_STRING) {
                result.type = TYPE_NUMBER;
                result.data.num = value_to_number(var_val);
            } else {
                result.type = TYPE_NUMBER;
                result.data.num = 0.0;
            }
            free_value(&var_val);
            return result;
        }
    }

    /* ----------------------------------------------------------------
     * Parentheses block / function call arguments
     * When we reach '(' we may have:
     *   a) A block of ';'-separated statements (returns last assigned var)
     *   b) A comma-separated list (collected by the function caller)
     * Here we handle the block case; comma lists are handled in function
     * call parsing above. Inside a paren block:
     *   - Statements separated by ';' are executed in sequence
     *   - The result is the value of the variable assigned on the last
     *     statement (or the expression value if no assignment).
     * ---------------------------------------------------------------- */
    if (ctx->expr[ctx->pos] == '(') {
        ctx->pos++;  /* skip '(' */

        Value last_result;
        last_result.type = TYPE_UNDEFINED;

        while (1) {
            skip_whitespace(ctx);

            /* End of block */
            if (ctx->expr[ctx->pos] == ')' || ctx->expr[ctx->pos] == '\0')
                break;

            /* Free previous iteration result */
            free_value(&last_result);

            /*
             * Detect statement type inside a paren block:
             *
             * 1. VAR = expr   -> explicit assignment (always assigns)
             * 2. VAR expr     -> implicit assignment (always assigns, e.g. B42)
             * 3. VAR op expr  -> self-referential ONLY when followed by ';'
             *                    (B+1; means B=B+1), plain value when last item
             * 4. anything else -> plain expression, no assignment
             *
             * This logic applies to both uppercase A-Z and '_'.
             */
            if (IS_VARNAME(ctx->expr[ctx->pos])) {
                int var_idx = VARIDX(ctx->expr[ctx->pos]);
                /* Peek at what follows the variable letter (skip spaces) */
                int peek = ctx->pos + 1;
                while (ctx->expr[peek] == ' ' || ctx->expr[peek] == '\t') peek++;
                char nc = ctx->expr[peek];

                /* Case 1: explicit '=' inside parens:
                 *   (A=expr)   -> comparison: 1 if A==expr, 0 otherwise
                 *   (A=expr;)  -> assignment: assigns expr to A            */
                if (nc == '=') {
                    Value cur = get_variable(var_idx);
                    ctx->pos++;          /* consume VAR letter */
                    skip_whitespace(ctx);
                    ctx->pos++;          /* consume '=' */
                    Value rhs = evaluate_expression(ctx);
                    skip_whitespace(ctx);
                    if (ctx->expr[ctx->pos] == ';') {
                        /* Semicolon present: treat as assignment */
                        free_value(&cur);
                        set_variable(var_idx, rhs);
                        free_value(&rhs);
                        last_result = copy_value(variables[var_idx]);
                    } else {
                        /* No semicolon: treat as equality comparison */
                        double eq;
                        if (cur.type == TYPE_STRING && rhs.type == TYPE_STRING)
                            eq = strcmp(cur.data.str, rhs.data.str) == 0 ? 1.0 : 0.0;
                        else
                            eq = value_to_number(cur) == value_to_number(rhs) ? 1.0 : 0.0;
                        free_value(&cur);
                        free_value(&rhs);
                        last_result.type = TYPE_NUMBER;
                        last_result.data.num = eq;
                    }
                }
                /* Case 2: value-starter -> implicit assignment (e.g. A42) */
                else if (nc != '\0' && nc != ')' && nc != ';' && nc != ',' &&
                     nc != '+' && nc != '*' && nc != '/' && nc != '%' &&
                     nc != '^' && nc != '&' && nc != '|' && nc != '<' &&
                     nc != '>' && nc != '!' &&
                     (nc != '-' || isdigit((unsigned char)ctx->expr[peek+1]) ||
                      ctx->expr[peek+1] == '(')) {
                    ctx->pos++;          /* consume VAR letter */
                    skip_whitespace(ctx);
                    Value val = evaluate_expression(ctx);
                    set_variable(var_idx, val);
                    free_value(&val);
                    last_result = copy_value(variables[var_idx]);
                }
                /* Case 3: binary operator follows -> self-referential */
                else if (nc == '+' || nc == '-' || nc == '*' || nc == '/' ||
                     nc == '%' || nc == '^' || nc == '&' || nc == '|' ||
                     nc == '<' || nc == '>') {
                    /* Build synthetic "VAR op expr" and evaluate it */
                    char synth[MAX_LINE_LENGTH + 2];
                    synth[0] = (char)VARCHAR(var_idx);
                    strncpy(synth + 1, ctx->expr + ctx->pos + 1, MAX_LINE_LENGTH - 1);
                    synth[MAX_LINE_LENGTH] = '\0';
                    /* Advance ctx past the whole expression */
                    ctx->pos++;  /* skip VAR letter */
                    Value rhs_dummy = evaluate_expression(ctx);
                    /* Compute the full self-ref value */
                    ParseContext ctx2;
                    ctx2.expr = synth;
                    ctx2.pos = 0;
                    ctx2.line_num = ctx->line_num;
                    Value full_val = evaluate_expression(&ctx2);
                    free_value(&rhs_dummy);

                    skip_whitespace(ctx);
                    /* Only assign if ';' follows (not last item) */
                    if (ctx->expr[ctx->pos] == ';') {
                        set_variable(var_idx, full_val);
                        last_result = copy_value(variables[var_idx]);
                    } else {
                        last_result = full_val;
                        full_val.type = TYPE_UNDEFINED; /* prevent double free */
                    }
                    free_value(&full_val);
                } else {
                    /* Case 4: plain expression starting with a variable */
                    last_result = evaluate_expression(ctx);
                }
            } else {
                /* Not a variable name: plain expression */
                last_result = evaluate_expression(ctx);
            }

            skip_whitespace(ctx);

            /* Consume statement separator: ';' or ',' both continue the block */
            if (ctx->expr[ctx->pos] == ';' || ctx->expr[ctx->pos] == ',') {
                ctx->pos++;
                continue;
            }
            /* Any other character (including ')') ends the block */
            break;
        }

        skip_whitespace(ctx);
        if (ctx->expr[ctx->pos] == ')')
            ctx->pos++;

        if (last_result.type == TYPE_UNDEFINED) {
            last_result.type = TYPE_NUMBER;
            last_result.data.num = 0.0;
        }

        return last_result;
    }

    /* ----------------------------------------------------------------
     * String literal
     * ---------------------------------------------------------------- */
    if (ctx->expr[ctx->pos] == '"') {
        ctx->pos++;
        int start = ctx->pos;
        while (ctx->expr[ctx->pos] && ctx->expr[ctx->pos] != '"') {
            if (ctx->expr[ctx->pos] == '\\' && ctx->expr[ctx->pos + 1])
                ctx->pos += 2;
            else
                ctx->pos++;
        }
        int len = ctx->pos - start;
        result.type = TYPE_STRING;
        result.data.str = (char *)malloc(len + 1);
        strncpy(result.data.str, ctx->expr + start, len);
        result.data.str[len] = '\0';
        if (ctx->expr[ctx->pos] == '"') ctx->pos++;
        return result;
    }

    /* ----------------------------------------------------------------
     * Random number (apostrophe)
     * '          -> random double in [0, 0.999999]
     * 'expr      -> set RNG seed to (int)expr, then return 0
     * ---------------------------------------------------------------- */
    if (ctx->expr[ctx->pos] == '\'') {
        ctx->pos++;

        skip_whitespace(ctx);
        if (isdigit((unsigned char)ctx->expr[ctx->pos]) ||
            IS_VARNAME(ctx->expr[ctx->pos]) ||
            ctx->expr[ctx->pos] == '(') {
            /* Parse seed value */
            Value seed_val = parse_primary(ctx);
            int seed = (int)value_to_number(seed_val);
            free_value(&seed_val);
            srand((unsigned int)seed);
            result.type = TYPE_NUMBER;
            result.data.num = 0.0;
        } else {
            /* Generate random in [0, 0.999999] */
            result.type = TYPE_NUMBER;
            result.data.num = (double)rand() / ((double)RAND_MAX + 1.0);
        }
        return result;
    }

    /* ----------------------------------------------------------------
     * Keyboard buffer read (colon in expression context)
     * Returns ASCII code of key in buffer, or 0 if buffer is empty.
     * Non-blocking (uses PDCurses nodelay).
     * ---------------------------------------------------------------- */
    if (ctx->expr[ctx->pos] == ':') {
        ctx->pos++;
        result.type = TYPE_NUMBER;
        nodelay(stdscr, TRUE);
        int key = wgetch(stdscr);
        nodelay(stdscr, FALSE);
        if (key == KEY_MOUSE) {
            mmask_t bstate = getmouse();
            request_mouse_pos();
            g_tmouse_x = Mouse_status.x;
            g_tmouse_y = Mouse_status.y;
            if (bstate & BUTTON1_PRESSED) { g_tmouse_click = 1; g_tmouse_drag |= 1; }
            if (bstate & BUTTON2_PRESSED) { g_tmouse_click = 2; g_tmouse_drag |= 2; }
            if (bstate & BUTTON3_PRESSED) { g_tmouse_click = 3; g_tmouse_drag |= 4; }
            if (bstate & BUTTON1_RELEASED) g_tmouse_drag &= ~1;
            if (bstate & BUTTON2_RELEASED) g_tmouse_drag &= ~2;
            if (bstate & BUTTON3_RELEASED) g_tmouse_drag &= ~4;
            result.data.num = 0.0;
        } else {
            result.data.num = (key == ERR) ? 0.0 : (double)key;
        }
        return result;
    }

    /* ----------------------------------------------------------------
     * Input variable (question mark inside expression)
     * ---------------------------------------------------------------- */
    if (ctx->expr[ctx->pos] == '?') {
        ctx->pos++;
        char input[MAX_LINE_LENGTH];
        memset(input, 0, sizeof(input));
        if (repl_mode) {
            printw("> ");
            refresh();
        }
        echo();
        wgetnstr(stdscr, input, sizeof(input) - 1);
        noecho();
        result.type = TYPE_STRING;
        result.data.str = _strdup(input);
        return result;
    }

    /* ----------------------------------------------------------------
     * Line number variable (#)
     * ---------------------------------------------------------------- */
    if (ctx->expr[ctx->pos] == '#') {
        ctx->pos++;
        result.type = TYPE_NUMBER;
        result.data.num = (double)ctx->line_num;
        return result;
    }

    /* ----------------------------------------------------------------
     * Array access (@index)
     * ---------------------------------------------------------------- */
    if (ctx->expr[ctx->pos] == '@') {
        ctx->pos++;
        Value index_val = parse_primary(ctx);
        int index = (int)value_to_number(index_val);
        free_value(&index_val);
        if (index < 0) index = 0;
        result.type = TYPE_NUMBER;
        result.data.num = (index < array_size) ? array_data[index] : 0.0;
        return result;
    }

    /* ----------------------------------------------------------------
     * Lowercase letter sequence -> screen function or math function call
     * Screen functions (gotoxy, putch, getch, setfore, setback, setattr,
     *   getw, geth, clear) receive Value arguments to allow strings.
     * All other lowercase sequences are dispatched to call_math_function.
     * e.g.  sin(A)  sqrt(B)  atan2(Y,X)  gotoxy(10,5)
     * ---------------------------------------------------------------- */
    if (islower((unsigned char)ctx->expr[ctx->pos])) {
        char func_name[64];
        int fi = 0;
        /* Consume lowercase letters AND digits so log10, log2, atan2 all work */
        while ((islower((unsigned char)ctx->expr[ctx->pos]) ||
                isdigit((unsigned char)ctx->expr[ctx->pos])) && fi < 63) {
            func_name[fi++] = ctx->expr[ctx->pos++];
        }
        func_name[fi] = '\0';

        skip_whitespace(ctx);

        /* Followed by '(' -> function call with arguments */
        if (ctx->expr[ctx->pos] == '(') {
            ctx->pos++;  /* skip '(' */

            if (is_screen_function(func_name)) {
                /* Screen functions: collect arguments as Value (allow strings) */
                Value vargs[MAX_FUNC_ARGS];
                int vnargs = 0;

                while (ctx->expr[ctx->pos] != ')' && ctx->expr[ctx->pos] != '\0') {
                    skip_whitespace(ctx);
                    if (ctx->expr[ctx->pos] == ')' || ctx->expr[ctx->pos] == '\0') break;
                    if (vnargs < MAX_FUNC_ARGS)
                        vargs[vnargs++] = evaluate_expression(ctx);
                    else {
                        Value dummy = evaluate_expression(ctx);
                        free_value(&dummy);
                    }
                    skip_whitespace(ctx);
                    if (ctx->expr[ctx->pos] == ',') ctx->pos++;
                }
                if (ctx->expr[ctx->pos] == ')') ctx->pos++;

                Value sresult = call_screen_function(func_name, vargs, vnargs);
                for (int i = 0; i < vnargs; i++) free_value(&vargs[i]);
                return sresult;

            } else {
                /* Math functions: collect arguments as doubles */
                double args[MAX_FUNC_ARGS];
                int nargs = 0;

                while (ctx->expr[ctx->pos] != ')' && ctx->expr[ctx->pos] != '\0') {
                    skip_whitespace(ctx);
                    if (ctx->expr[ctx->pos] == ')' || ctx->expr[ctx->pos] == '\0') break;
                    Value arg_val = evaluate_expression(ctx);
                    if (nargs < MAX_FUNC_ARGS)
                        args[nargs++] = value_to_number(arg_val);
                    free_value(&arg_val);
                    skip_whitespace(ctx);
                    if (ctx->expr[ctx->pos] == ',') ctx->pos++;
                }
                if (ctx->expr[ctx->pos] == ')') ctx->pos++;

                return call_math_function(func_name, args, nargs);
            }
        } else {
            /* Zero-arg call (e.g. pi, e, getw, geth, getch, clear) */
            if (is_screen_function(func_name)) {
                return call_screen_function(func_name, NULL, 0);
            } else {
                double dummy_args[1];
                return call_math_function(func_name, dummy_args, 0);
            }
        }
    }

    /* ----------------------------------------------------------------
     * Single-letter variable: A-Z or '_'
     * ---------------------------------------------------------------- */
    if (IS_VARNAME(ctx->expr[ctx->pos])) {
        int var_idx = VARIDX(ctx->expr[ctx->pos]);
        ctx->pos++;
        return get_variable(var_idx);
    }

    /* ----------------------------------------------------------------
     * Numeric literal
     * ---------------------------------------------------------------- */
    if (isdigit((unsigned char)ctx->expr[ctx->pos]) || ctx->expr[ctx->pos] == '.') {
        char *endptr;
        result.type = TYPE_NUMBER;
        result.data.num = strtod(ctx->expr + ctx->pos, &endptr);
        ctx->pos = (int)(endptr - ctx->expr);
        return result;
    }

    /* Unknown -> return 0 */
    result.type = TYPE_NUMBER;
    result.data.num = 0.0;
    return result;
}

/* ------------------------------------------------------------------ */
/* Evaluate expression (left-to-right with binary operators)          */
/* ------------------------------------------------------------------ */
Value evaluate_expression(ParseContext *ctx) {
    Value left = parse_primary(ctx);

    while (1) {
        skip_whitespace(ctx);

        char op = ctx->expr[ctx->pos];

        /* End of expression */
        if (op == '\0' || op == ')' || op == ';' || op == ',')
            break;

        if (op == '+' || op == '-' || op == '*' || op == '/' || op == '%' ||
            op == '^' || op == '&' || op == '|' || op == '<' || op == '>' || op == '=') {
            ctx->pos++;
            Value right = parse_primary(ctx);
            Value new_left;

            /* String concatenation with '+' */
            if (op == '+' && (left.type == TYPE_STRING || right.type == TYPE_STRING)) {
                char *ls = value_to_string(left);
                char *rs = value_to_string(right);
                new_left.type = TYPE_STRING;
                new_left.data.str = (char *)malloc(strlen(ls) + strlen(rs) + 1);
                strcpy(new_left.data.str, ls);
                strcat(new_left.data.str, rs);
                free(ls); free(rs);
            } else {
                double ln = value_to_number(left);
                double rn = value_to_number(right);
                new_left.type = TYPE_NUMBER;

                switch (op) {
                    case '+': new_left.data.num = ln + rn; break;
                    case '-': new_left.data.num = ln - rn; break;
                    case '*': new_left.data.num = ln * rn; break;
                    case '/':
                        new_left.data.num = (rn == 0.0) ? 0.0 : ln / rn;
                        if (rn == 0.0) { printw("Error: Division by zero\n"); refresh(); }
                        break;
                    case '%':
                        new_left.data.num = (rn == 0.0) ? 0.0 : fmod(ln, rn);
                        if (rn == 0.0) { printw("Error: Modulo by zero\n"); refresh(); }
                        break;
                    case '^': new_left.data.num = pow(ln, rn); break;
                    case '&': new_left.data.num = (ln != 0.0 && rn != 0.0) ? 1.0 : 0.0; break;
                    case '|': new_left.data.num = (ln != 0.0 || rn != 0.0) ? 1.0 : 0.0; break;
                    case '<': new_left.data.num = (ln < rn) ? 1.0 : 0.0; break;
                    case '>': new_left.data.num = (ln > rn) ? 1.0 : 0.0; break;
                    case '=': new_left.data.num = (ln == rn) ? 1.0 : 0.0; break;
                    default:  new_left.data.num = 0.0; break;
                }
            }

            free_value(&left);
            free_value(&right);
            left = new_left;
        } else {
            break;
        }
    }

    return left;
}

/* ------------------------------------------------------------------ */
/* Execute a REPL command (the part after ':')                        */
/* Returns 1 if the command was handled, 0 if unknown.                */
/* ------------------------------------------------------------------ */
int execute_repl_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        print_repl_help();
        return 1;
    }
    if (strcmp(cmd, "syntax") == 0) {
        print_repl_syntax_help();
        return 1;
    }
    if (strcmp(cmd, "screen") == 0) {
        print_repl_screen_help();
        return 1;
    }
    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        printw("Arrivederci!\n");
        refresh();
        endwin();
        cleanup_interpreter();
        exit(0);
    }
    if (strcmp(cmd, "vars") == 0) {
        int found = 0;
        for (int i = 0; i < NUM_VARS; i++) {
            if (variables[i].type != TYPE_UNDEFINED) {
                printw("%c = ", (char)VARCHAR(i));
                if (variables[i].type == TYPE_NUMBER)
                    printw("%.15g\n", variables[i].data.num);
                else
                    printw("\"%s\"\n", variables[i].data.str);
                found = 1;
            }
        }
        if (!found) printw("No variables defined.\n");
        refresh();
        return 1;
    }
    if (strcmp(cmd, "array") == 0) {
        if (array_size == 0) {
            printw("Array is empty.\n");
        } else {
            printw("Array (size: %d):\n", array_size);
            for (int i = 0; i < array_size && i < 20; i++)
                printw("  @%d = %.15g\n", i, array_data[i]);
            if (array_size > 20)
                printw("  ... (%d elements total)\n", array_size);
        }
        refresh();
        return 1;
    }
    if (strcmp(cmd, "lines") == 0) {
        if (line_count == 0) {
            printw("No lines in program.\n");
        } else {
            printw("Program (%d lines):\n", line_count);
            for (int i = 0; i < line_count && i < 50; i++)
                printw("  %3d: %s\n", i + 1, source_lines[i]);
            if (line_count > 50)
                printw("  ... (%d lines total)\n", line_count);
        }
        refresh();
        return 1;
    }
    if (strcmp(cmd, "clear") == 0) {
        for (int i = 0; i < NUM_VARS; i++) {
            if (variables[i].type == TYPE_STRING && variables[i].data.str)
                free(variables[i].data.str);
            variables[i].type = TYPE_UNDEFINED;
        }
        if (array_data) { free(array_data); array_data = NULL; }
        array_size = 0;
        printw("All variables and array cleared.\n");
        refresh();
        return 1;
    }
    if (strcmp(cmd, "reset") == 0) {
        for (int i = 0; i < NUM_VARS; i++) {
            if (variables[i].type == TYPE_STRING && variables[i].data.str)
                free(variables[i].data.str);
            variables[i].type = TYPE_UNDEFINED;
        }
        if (array_data) { free(array_data); array_data = NULL; }
        array_size = 0;
        if (source_lines) {
            for (int i = 0; i < line_count; i++)
                free(source_lines[i]);
            free(source_lines);
            source_lines = NULL;
        }
        line_count = 0;
        printw("REPL completely reset.\n");
        refresh();
        return 1;
    }
    if (strncmp(cmd, "debug ", 6) == 0) {
        char var_name = cmd[6];
        if ((var_name >= 'A' && var_name <= 'Z') || var_name == '_') {
            int idx = (var_name == '_') ? 26 : (var_name - 'A');
            if (variables[idx].type == TYPE_STRING) {
                printw("Variable %c (string):\n", var_name);
                printw("  Content: \"");
                for (int i = 0; variables[idx].data.str[i]; i++)
                    printw("%c", variables[idx].data.str[i]);
                printw("\"\n  Bytes (hex): ");
                for (int i = 0; variables[idx].data.str[i]; i++)
                    printw("%02X ", (unsigned char)variables[idx].data.str[i]);
                printw("\n  Bytes (dec): ");
                for (int i = 0; variables[idx].data.str[i]; i++)
                    printw("%d ", (unsigned char)variables[idx].data.str[i]);
                printw("\n");
            } else if (variables[idx].type == TYPE_NUMBER) {
                printw("Variable %c = %.15g (number)\n", var_name, variables[idx].data.num);
            } else {
                printw("Variable %c is undefined\n", var_name);
            }
            refresh();
        } else {
            printw("Usage: :debug VARIABLE (e.g. :debug A or :debug _)\n");
            refresh();
        }
        return 1;
    }

    return 0;  /* unknown command */
}

/* ------------------------------------------------------------------ */
/* Execute a single program line                                       */
/* ------------------------------------------------------------------ */
void execute_line(int line_num) {
    if (line_num < 1 || line_num > line_count) return;

    current_line = line_num;
    const char *line = source_lines[line_num - 1];

    ParseContext ctx;
    ctx.expr = line;
    ctx.pos = 0;
    ctx.line_num = line_num;

    skip_whitespace(&ctx);

    /* Empty line */
    if (ctx.expr[ctx.pos] == '\0') return;

    /* -----------------------------------------------------------
     * REPL command (:command) - works both in REPL and in programs
     * ----------------------------------------------------------- */
    if (ctx.expr[ctx.pos] == ':') {
        ctx.pos++;
        char cmd_buf[256];
        int ci = 0;
        while (ctx.expr[ctx.pos] != '\0' && ci < 255)
            cmd_buf[ci++] = ctx.expr[ctx.pos++];
        cmd_buf[ci] = '\0';

        if (!execute_repl_command(cmd_buf))
            printw("Unknown command: :%s\n", cmd_buf);
        refresh();
        return;
    }

    /* -----------------------------------------------------------
     * Print statement: starts with '?'
     * ----------------------------------------------------------- */
    if (ctx.expr[ctx.pos] == '?') {
        ctx.pos++;
        while (ctx.expr[ctx.pos] == ' ' || ctx.expr[ctx.pos] == '\t') ctx.pos++;
        if (ctx.expr[ctx.pos] == '=') ctx.pos++;
        Value result = evaluate_expression(&ctx);
        apply_attrs();
        if (result.type == TYPE_NUMBER) {
            printw("%.15g", result.data.num);
            refresh();
            need_newline = 1;
        } else if (result.type == TYPE_STRING) {
            print_escaped_string(result.data.str);
            const char *s = result.data.str;
            int slen = strlen(s);
            if (slen == 0)
                ; /* no output, keep flag as-is */
            else if (s[slen - 1] == '\n')
                need_newline = 0;
            else if (slen >= 2 && s[slen - 2] == '\\' && s[slen - 1] == 'n')
                need_newline = 0;
            else
                need_newline = 1;
        }

        free_value(&result);
        return;
    }

    /* -----------------------------------------------------------
     * Array assignment: expr@index = value
     * ----------------------------------------------------------- */
    if (isdigit((unsigned char)ctx.expr[ctx.pos]) || IS_VARNAME(ctx.expr[ctx.pos])) {
        int start_pos = ctx.pos;
        Value index_val = parse_primary(&ctx);

        skip_whitespace(&ctx);

        if (ctx.expr[ctx.pos] == '@') {
            ctx.pos++;
            int index = (int)value_to_number(index_val);
            free_value(&index_val);
            if (index < 0) index = 0;

            /* Expand array if needed */
            if (index >= array_size) {
                int new_size = index + 1;
                array_data = (double *)realloc(array_data, new_size * sizeof(double));
                for (int i = array_size; i < new_size; i++) array_data[i] = 0.0;
                array_size = new_size;
            }

            skip_whitespace(&ctx);
            if (ctx.expr[ctx.pos] == '=') ctx.pos++;

            Value val = evaluate_expression(&ctx);
            array_data[index] = value_to_number(val);

            if (repl_mode && show_assignments)
                printw("< @%d = %.15g\n", index, array_data[index]);

            free_value(&val);
            return;
        }

        /* Not an array assignment - reset and fall through */
        ctx.pos = start_pos;
        free_value(&index_val);
    }

    /* -----------------------------------------------------------
     * Variable assignment: VAR = expr  (VAR is A-Z or '_')
     * ----------------------------------------------------------- */
    if (IS_VARNAME(ctx.expr[ctx.pos])) {
        int var_idx = VARIDX(ctx.expr[ctx.pos]);
        ctx.pos++;

        skip_whitespace(&ctx);

        /* Bare variable name -> make it undefined */
        if (ctx.expr[ctx.pos] == '\0') {
            if (variables[var_idx].type == TYPE_STRING && variables[var_idx].data.str)
                free(variables[var_idx].data.str);
            variables[var_idx].type = TYPE_UNDEFINED;
            if (repl_mode && show_assignments)
                printw("< %c = undefined\n", (char)VARCHAR(var_idx));
            refresh();
            return;
        }

        /* Explicit '=' -> normal assignment */
        if (ctx.expr[ctx.pos] == '=') {
            ctx.pos++;
            Value val = evaluate_expression(&ctx);
            set_variable(var_idx, val);
            free_value(&val);
            return;
        }

        /* Self-referential shorthand: VAR op expr  means  VAR = VAR op expr
         * Detected when the next character is a binary operator but NOT '='.
         * Build a synthetic expression "VARop..." and evaluate it.
         * Examples: A+1  ->  A=A+1
         *           A*(2+B)  ->  A=A*(2+B)                               */
        if (ctx.expr[ctx.pos] == '+' || ctx.expr[ctx.pos] == '-' ||
            ctx.expr[ctx.pos] == '*' || ctx.expr[ctx.pos] == '/' ||
            ctx.expr[ctx.pos] == '%' || ctx.expr[ctx.pos] == '^' ||
            ctx.expr[ctx.pos] == '&' || ctx.expr[ctx.pos] == '|' ||
            ctx.expr[ctx.pos] == '<' || ctx.expr[ctx.pos] == '>') {
            char synth[MAX_LINE_LENGTH + 2];
            synth[0] = (char)VARCHAR(var_idx);
            strncpy(synth + 1, ctx.expr + ctx.pos, MAX_LINE_LENGTH - 1);
            synth[MAX_LINE_LENGTH] = '\0';
            ParseContext ctx2;
            ctx2.expr = synth;
            ctx2.pos = 0;
            ctx2.line_num = ctx.line_num;
            Value val = evaluate_expression(&ctx2);
            set_variable(var_idx, val);
            free_value(&val);
            return;
        }

        /* Implicit assignment: VAR expr  means  VAR = expr  (e.g. A42) */
        Value val = evaluate_expression(&ctx);
        set_variable(var_idx, val);
        free_value(&val);
        return;
    }

    /* -----------------------------------------------------------
     * Line jump: #=expr
     * ----------------------------------------------------------- */
    if (ctx.expr[ctx.pos] == '#') {
        ctx.pos++;
        skip_whitespace(&ctx);
        if (ctx.expr[ctx.pos] == '=') ctx.pos++;

        Value val = evaluate_expression(&ctx);
        int new_line = (int)value_to_number(val);
        free_value(&val);

        if (new_line > 0 && new_line <= line_count) {
            current_line = new_line - 1; /* will be incremented by caller */
        }
        return;
    }

    /* -----------------------------------------------------------
     * Bare expression (for side effects)
     * ----------------------------------------------------------- */
    Value val = evaluate_expression(&ctx);
    free_value(&val);
}

/* ------------------------------------------------------------------ */
/* Execute program from a given line                                   */
/* ------------------------------------------------------------------ */
void execute_from_line(int start_line) {
    for (current_line = start_line; current_line <= line_count; current_line++) {
        if (g_interrupted) {
            g_interrupted = 0;
            printw("\n[Interrupted]\n");
            refresh();
            break;
        }
        execute_line(current_line);
    }
}

/* Execute the entire program */
void execute_program(void) {
    execute_from_line(1);
}

/* ------------------------------------------------------------------ */
/* REPL help text                                                      */
/* ------------------------------------------------------------------ */
void print_repl_help(void) {
    printw("ITL REPL - Special commands:\n");
    printw("  :help         - Show this help\n");
    printw("  :vars         - Show all defined variables\n");
    printw("  :clear        - Clear all variables\n");
    printw("  :array        - Show array contents\n");
    printw("  :lines        - Show program lines\n");
    printw("  :syntax       - Show syntax help\n");
    printw("  :screen       - Show screen functions help\n");
    printw("  :debug VAR    - Show raw bytes of a variable (e.g. :debug A or :debug _)\n");
    printw("  :reset        - Reset the REPL completely (clears everything)\n");
    printw("  :exit/:quit   - Exit the REPL\n");
    printw("\n");
    printw("Line editing keys:\n");
    printw("  Left/Right    - Move cursor\n");
    printw("  Home/End      - Jump to start/end of line\n");
    printw("  Backspace/Del - Delete character before/at cursor\n");
    printw("  Up/Down       - Navigate command history\n");
    printw("\n");
    refresh();
}

/* ------------------------------------------------------------------ */
/* REPL help Syntax text                                                      */
/* ------------------------------------------------------------------ */
void print_repl_syntax_help(void) {
    printw("ITL syntax:\n");
    printw("  #              - Current line number\n");
    printw("  #=expr         - Jump to line expr\n");
    printw("  '              - Random number [0, 0.999999]\n");
    printw("  'N             - Set RNG seed to integer N\n");
    printw("  :              - Read key from keyboard buffer (0 if empty)\n");
    printw("  ?              - Input from keyboard (inside expression)\n");
    printw("  $VAR           - Type conversion\n");
    printw("  @index         - Array access\n");
    printw("  ;              - Statement separator\n");
    printw("  func(args)     - Math function call (sin, cos, sqrt, etc.)\n");
    printw("  (stmt;stmt)    - Block: execute stmts, return last var value\n");
    printw("  _              - Underscore variable (27th single-letter var)\n");
    printw("\n");
    refresh();
}

void print_repl_screen_help(void) {
    printw("Screen functions:\n");
    printw("  gotoxy(x,y)    - Move cursor to column x, row y\n");
    printw("  putch(c)       - Write char (ASCII or string) at cursor\n");
    printw("  getch()        - Read char at cursor (returns ASCII code)\n");
    printw("  setfore(c)     - Set foreground color 0-7\n");
    printw("  setback(c)     - Set background color 0-7\n");
    printw("  setattr(a)     - Set attribute: 0=normal, 1=bold, 2=reverse\n");
    printw("  getw()         - Screen width in columns\n");
    printw("  geth()         - Screen height in rows\n");
    printw("  clear()        - Clear screen with current background color\n");
    printw("\n");
    refresh();
}

/* ------------------------------------------------------------------ */
/* REPL line-editor with history                                       */
/* ------------------------------------------------------------------ */

static void repl_history_add(const char *line) {
    if (!line || !line[0]) return;
    /* Skip duplicate consecutive entries */
    if (repl_history_count > 0 &&
        strcmp(repl_history[repl_history_count - 1], line) == 0)
        return;
    if (repl_history_count == REPL_HISTORY_MAX) {
        free(repl_history[0]);
        memmove(repl_history, repl_history + 1,
                (REPL_HISTORY_MAX - 1) * sizeof(char *));
        repl_history_count--;
    }
    repl_history[repl_history_count++] = _strdup(line);
}

/*
 * repl_readline() -- interactive line editor built on PDCurses wgetch().
 *
 * Supports:
 *   Left / Right    -- move cursor
 *   Home / End      -- jump to beginning / end of line
 *   Backspace       -- delete character before cursor
 *   Del             -- delete character at cursor
 *   Up / Down       -- navigate command history
 *   Printable chars -- insert at cursor position
 *
 * The prompt must already be printed and the cursor positioned at the
 * start of the editable area before calling this function.
 *
 * Returns 1 on success (Enter pressed), 0 on EOF / hard error.
 */
static int repl_readline(char *buf, int maxlen) {
    int len = 0;          /* current string length   */
    int pos = 0;          /* cursor position in buf  */
    int hist_idx;         /* current history position */
    char saved[MAX_LINE_LENGTH]; /* line saved while browsing history */

    buf[0]   = '\0';
    saved[0] = '\0';
    hist_idx = repl_history_count; /* one past the last entry = "current" */

    /* Remember where the editable area starts on screen */
    int prompt_y, prompt_x;
    getyx(stdscr, prompt_y, prompt_x);

    keypad(stdscr, TRUE);   /* enable arrow / function key decoding */
    curs_set(1);

    while (1) {
        /* ---- Redraw the editable line -------------------------------- */
        {
            /* How many terminal columns does the prompt occupy? */
            int cols = COLS > 0 ? COLS : 80;

            /* Total length of text starting at prompt_x */
            int full_len = prompt_x + len;
            int rows_used = full_len / cols + 1;

            /* Clear all rows occupied by the current input */
            for (int r = 0; r < rows_used; r++) {
                move(prompt_y + r, (r == 0) ? prompt_x : 0);
                clrtoeol();
            }

            /* Redraw characters */
            move(prompt_y, prompt_x);
            for (int i = 0; i < len; i++)
                addch((unsigned char)buf[i]);

            /* Place the cursor at the logical position */
            int abs_cur = prompt_x + pos;
            move(prompt_y + abs_cur / cols, abs_cur % cols);
            refresh();
        }

        /* ---- Read one key ------------------------------------------- */
        int ch = wgetch(stdscr);

        if (ch == ERR) {
            keypad(stdscr, FALSE);
            curs_set(0);
            return 0;               /* EOF / error */
        }

        /* Enter (CR or LF) */
        if (ch == '\r' || ch == '\n' || ch == KEY_ENTER) {
            buf[len] = '\0';
            /* Advance to a fresh line */
            move(prompt_y, prompt_x);
            for (int i = 0; i < len; i++) addch((unsigned char)buf[i]);
            addch('\n');
            refresh();
            break;
        }

        /* Ctrl+C -- treat as cancellation, return empty line */
        if (ch == 3) {
            buf[0] = '\0';
            addch('\n');
            refresh();
            break;
        }

        /* Backspace */
        if (ch == KEY_BACKSPACE || ch == '\b' || ch == 127) {
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos);
                pos--;
                len--;
                buf[len] = '\0';
            }
            continue;
        }

        /* Delete (Del key) */
        if (ch == KEY_DC) {
            if (pos < len) {
                memmove(buf + pos, buf + pos + 1, len - pos - 1);
                len--;
                buf[len] = '\0';
            }
            continue;
        }

        /* Left arrow */
        if (ch == KEY_LEFT) {
            if (pos > 0) pos--;
            continue;
        }

        /* Right arrow */
        if (ch == KEY_RIGHT) {
            if (pos < len) pos++;
            continue;
        }

        /* Home */
        if (ch == KEY_HOME) {
            pos = 0;
            continue;
        }

        /* End */
        if (ch == KEY_END) {
            pos = len;
            continue;
        }

        /* Up arrow -- older history entry */
        if (ch == KEY_UP) {
            if (hist_idx == repl_history_count) {
                /* Save what the user was typing */
                strncpy(saved, buf, maxlen - 1);
                saved[maxlen - 1] = '\0';
            }
            if (hist_idx > 0) {
                hist_idx--;
                strncpy(buf, repl_history[hist_idx], maxlen - 1);
                buf[maxlen - 1] = '\0';
                len = (int)strlen(buf);
                pos = len;
            }
            continue;
        }

        /* Down arrow -- newer history entry / back to current */
        if (ch == KEY_DOWN) {
            if (hist_idx < repl_history_count) {
                hist_idx++;
                if (hist_idx == repl_history_count) {
                    strncpy(buf, saved, maxlen - 1);
                } else {
                    strncpy(buf, repl_history[hist_idx], maxlen - 1);
                }
                buf[maxlen - 1] = '\0';
                len = (int)strlen(buf);
                pos = len;
            }
            continue;
        }

        /* Printable / extended character -- insert at cursor */
        if (ch >= 32 && ch < 256 && len < maxlen - 1) {
            memmove(buf + pos + 1, buf + pos, len - pos);
            buf[pos] = (char)ch;
            pos++;
            len++;
            buf[len] = '\0';
        }
    }

    keypad(stdscr, FALSE);
    curs_set(0);
    return 1;
}

/* ------------------------------------------------------------------ */
/* REPL main loop                                                      */
/* ------------------------------------------------------------------ */
void run_repl(void) {
    char input[MAX_LINE_LENGTH];

    repl_mode = 1;
    show_assignments = 1;

    printw("ITL (Incredibly Tiny Language) Advanced REPL v0.5.0\n");
    printw("Type ':help' for the list of commands.\n");
    printw("Type ':exit' to quit.\n\n");
    refresh();

    while (1) {
        /* If previous output didn't end with a newline, move to a new line
           before showing the prompt so it is always at the start of a line */
        if (need_newline) {
            addch('\n');
            need_newline = 0;
        }

        /* Show the NEXT line number as prompt */
        printw("%d> ", line_count + 1);
        refresh();

        /* Read a line using our custom line editor */
        memset(input, 0, sizeof(input));
        int ret = repl_readline(input, sizeof(input));

        if (!ret) break;  /* EOF or error (e.g. Ctrl+Z) */

        /* Remove trailing newline (safety) */
        int len = (int)strlen(input);
        while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r'))
            input[--len] = '\0';

        /* Skip empty lines */
        if (len == 0) continue;

        /* Add to history (before execution so history is always stored) */
        repl_history_add(input);

        /* Handle standalone REPL commands (begin with ':') */
        if (input[0] == ':') {
            if (!execute_repl_command(input + 1)) {
                printw("Unknown command: %s\n", input);
                printw("Type ':help' for the list of commands.\n");
                refresh();
            }
            continue;
        }

        /* Add line(s) to program and execute from the first new line */
        int start_line = line_count + 1;
        add_repl_line(input);

        if (start_line <= line_count)
            execute_from_line(start_line);
    }
}

/* ------------------------------------------------------------------ */
/* Main entry point                                                    */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    /* Initialise PDCurses ------------------------------------------ */
    initscr();
    start_color();
    init_color_pairs();

    /* Allow the window to scroll when output reaches the bottom edge  */
    scrollok(stdscr, TRUE);
    idlok(stdscr, TRUE);

    /* Do not translate function/arrow keys; keep raw key codes        */
    //keypad(stdscr, FALSE);
    /* Changed to get mouse events */
    keypad(stdscr, TRUE);

    /* Do not automatically echo typed characters (we control this     */
    /* explicitly when reading user input)                             */
    noecho();
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    /* Set default appearance: white text on black background          */
    /* Pair 8 = bg=0 (black), fg=7 (white)  per our pair formula      */
    itl_fg   = COLOR_WHITE;
    itl_bg   = COLOR_BLACK;
    itl_attr = A_NORMAL;
    apply_attrs();
    bkgd((chtype)(COLOR_PAIR(8) | ' '));
    wclear(stdscr);
    move(0, 0);
    refresh();

    /* Initialise the interpreter state ----------------------------- */
    init_interpreter();
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
    /* Re-enable processed input so Ctrl+C events are actually delivered,
       since PDCurses clears ENABLE_PROCESSED_INPUT on initialization. */
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD conMode;
    GetConsoleMode(hIn, &conMode);
    SetConsoleMode(hIn, conMode | ENABLE_PROCESSED_INPUT);

    if (argc > 1) {
        /* File mode */
        char filename[MAX_PATH];
        strncpy(filename, argv[1], MAX_PATH - 1);
        filename[MAX_PATH - 1] = '\0';

        repl_mode = 0;

        if (!load_source(filename)) {
            printw("Error: Cannot open file '%s'\n", filename);
            refresh();
            endwin();
            cleanup_interpreter();
            return 1;
        }

        execute_program();

        /* In file mode the PDCurses window would disappear the instant
         * endwin() is called, taking all output with it.  Give the user
         * a chance to read the screen before we restore the terminal. */
        if (need_newline) {
            addch('\n');
            need_newline = 0;
        }
        attron(A_REVERSE);
        printw(" Press any key to exit... ");
        attroff(A_REVERSE);
        refresh();
        keypad(stdscr, TRUE);   /* accept function/arrow keys too */
        wgetch(stdscr);
        keypad(stdscr, FALSE);
    } else {
        /* REPL mode */
        run_repl();
    }

    endwin();
    cleanup_interpreter();
    return 0;
}
