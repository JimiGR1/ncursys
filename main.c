/*
 * ncursys - A terminal-based tool writen in curses to visualize hard disk usage.
 * Copyright (C) 2026 Dimitris Trivizakis <dim.trivizakis@yandex.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <math.h>
#include <locale.h>
#include <getopt.h>
#include "path_data.h"
#include "path_tree.h"
#include "tree_view.h"
#include "errors.h"
#include "constants.h"
#include "helpers.h"

// this is passed to build_tree_thread
struct build_tree_struct {
    path_tree *pt;
    gint cancel;        // access only with g_atomic_int_*
    const char *path;
    int threads;
    bool one_file_system;
};

void*    build_tree(struct build_tree_struct *args);

void     fatal(const char *lib, const char *msg, int error_code);
void     init_curses(void);
bool     setup_windows(void);

void     select_row(int index);
bool     redraw(path_tree *pt, bool relayout);
void     draw_status(GNode *root, bool scan_finished);
void     draw_row(WINDOW *win, const view_row *row);
void     draw_scrollbar(void);

/* variables */

WINDOW  *brd_wnd,                           /* this refers to the top left boxed window */
        *list_win,                          /* this refers to the window inside brd_wnd showing the visible rows */
        *scrollbar_win;                     /* this refers to the vertical scrollbar on the rightmost column of list_win */

int     list_height,                        /* this is the number of rows list_win displays */
        list_width = OPT_LIST_WIDTH;        /* this is the number of columns list_win displays */

int     start_y = 1,                        /* this is the offset from world 0 row to brd_wnd zero row */
        start_x = 1;                        /* this is the offset from world 0 col to brd_wnd zero col */

tree_view *view;                            /* expanded directories and the visible rows */

int     top_row = 0,                        /* this is the row shown on the first line of list_win */
        selected = 0;                       /* this is the selected row */
GNode   *selected_node = NULL;              /* this is the entry the user selected - the selection follows it when rows re-sort, NULL until the user picks one */

bool    show_perc_bar   = true,             /* this is true if width can hold perc bar number */
        show_sizes      = true,             /* this is true if width can hold size number */
        show_items      = true,             /* this is true if width can hold item number */
        show_subdirs    = true,             /* this is true if width can hold subdir number */
        show_files      = true;             /* this is true if width can hold file number */

int main(int argc, char * argv[]) {

    // Set the locale to follow shell env
    setlocale(LC_ALL, "");
    // Define long options
    static struct option long_options[] = {
        {"threads", required_argument, 0, 't'},
        {"one-file-system", no_argument, 0, 'x'},
        {0, 0, 0, 0}
    };

    int opt, option_index, thread_number = g_get_num_processors() + 2;
    bool one_file_system = false;
    char *path = NULL;

    while ((opt = getopt_long(argc, argv, "t:x", long_options, &option_index)) != -1) {
        switch (opt) {
            case 't': {
                char *end;
                long n = strtol(optarg, &end, 10);
                if(*optarg == '\0' || *end != '\0' || n < 1 || n > MAX_THREADS) {
                    fprintf(stderr, "Invalid thread number '%s': expected a number from 1 to %d\n", optarg, MAX_THREADS);
                    return EXIT_FAILURE;
                }
                thread_number = (int)n;
                break;
            }
            case 'x':
                one_file_system = true;
                break;
            case '?':
                fprintf(stderr, "Unknown option or missing argument. Usage: %s path [--threads thread_number] [-x]\n", argv[0]);
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "Usage: %s path [--threads n_threads] [-x]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Remaining argument is the path
    if (optind < argc) {
        path = argv[optind];
    } else {
        fprintf(stderr, "Usage: %s path [--threads thread_number] [-x]\n", argv[0]);
        return EXIT_FAILURE;
    }

    init_curses();

    bool too_small = !setup_windows();      /* true while the terminal is below MIN_COLS x MIN_LINES */

    view = tree_view_new();
    path_tree *pt = path_tree_init();

    struct build_tree_struct build_thread_args = {pt, 0, path, thread_number, one_file_system};
    GThread *build_tree_thread = g_thread_new("build_tree_thread", (GThreadFunc)build_tree, &build_thread_args);

    // rows are re-laid out (sizes change while scanning) every REFRESH_INTERVAL, user input is applied instantly
    gint64 last_layout = 0;                 // monotonic time in microseconds - 0 forces a layout
    bool layout_final = false;              // the last layout saw the finished scan - nothing changes on its own anymore

    bool quit = false;
    while(!quit) {

        int ch = getch();                   // waits at most INPUT_POLL_INTERVAL

        if(ch == 'q') {
            quit = true;
            continue;
        }

        bool dirty = false;

        if(ch == KEY_RESIZE) {
            // ncurses has already updated LINES and COLS
            too_small = !setup_windows();
            dirty = true;
        }

        // nothing is drawn until the terminal is big enough again
        if(too_small)
            continue;

        gint64 now = g_get_monotonic_time();
        bool relayout = !layout_final && now - last_layout >= REFRESH_INTERVAL;
        dirty = dirty || relayout;
        int rows = (int)view->rows->len;

        switch(ch) {
            case KEY_UP:
                if(selected > 0) {
                    select_row(selected - 1);
                    dirty = true;
                }
                break;
            case KEY_DOWN:
                if(selected < rows - 1) {
                    select_row(selected + 1);
                    dirty = true;
                }
                break;
            case 'i':
                // TODO: handle print info popup-window
                break;
            case '\n':
                if(rows > 0) {
                    select_row(selected);
                    GNode *node = selected_node;
                    // is_directory never changes after the node is added - no lock needed
                    if(((path_data *)node->data)->is_directory) {
                        tree_view_toggle(view, node);
                        relayout = dirty = true;
                    }
                    // TODO; maybe show a popup for non directory entries with more details would be nice
                }
                break;
            default:
                break;
        }

        if(dirty) {
            bool complete = redraw(pt, relayout);
            if(relayout) {
                last_layout = now;
                layout_final = complete;
            }
        }

    }

    // clear curses to restore terminal
    flushinp();
    endwin();

    printf("Finishing...\n");

    g_atomic_int_set(&build_thread_args.cancel, 1);

    // Wait for the build thread to terminate
    g_thread_join(build_tree_thread);

    // cleanup
    tree_view_free(view);
    path_tree_destroy(pt);

    return 0;
}

void init_curses(void) {
    initscr();

    if(has_colors()) {
        use_default_colors();
        if(start_color() != OK)
            fatal("curses", "Terminal cannot start colors", CURSES_INIT_ERROR);
    }

    init_pair(TERM_DEFAULT, -1, -1);

    // resizes arrive as KEY_RESIZE from getch() through ncurses' own SIGWINCH handler
    noecho();                        // Don't echo any keypresses
    cbreak();                        // Don't wait for enter; pass back every keypress
    timeout(INPUT_POLL_INTERVAL);    // getch waits at most this long for a key
    keypad(stdscr, true);            // Enable arrow keys
    curs_set(0);                     // hide main cursor
    // Enable mouse events
    //mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
}

/* (re)creates the windows for the current terminal size - returns false (and shows a notice) if the terminal is too small */
bool setup_windows(void) {

    if(scrollbar_win != NULL) { delwin(scrollbar_win); scrollbar_win = NULL; }
    if(list_win != NULL)      { delwin(list_win);      list_win = NULL; }
    if(brd_wnd != NULL)       { delwin(brd_wnd);       brd_wnd = NULL; }

    clear();

    if(COLS < MIN_COLS || LINES < MIN_LINES) {
        mvprintw(0, 0, "Terminal too small: %dx%d, need at least %dx%d ('q' to exit)", COLS, LINES, MIN_COLS, MIN_LINES);
        refresh();
        return false;
    }

    list_width = MIN(OPT_LIST_WIDTH, COLS - 2);
    list_height = LINES - HEADER_HEIGHT - 2;

    show_files = COLS >= SHOW_FILES_COL_THRESHOLD;
    show_subdirs = COLS >= SHOW_SUBDIRS_COL_THRESHOLD;
    show_items = COLS >= SHOW_ITEMS_COL_THRESHOLD;
    show_sizes = COLS >= SHOW_SIZES_COL_THRESHOLD;
    show_perc_bar = COLS >= SHOW_PERC_BAR_COL_THRESHOLD;

    brd_wnd = newwin(list_height + 2, list_width + 2, start_y - 1, start_x - 1);
    list_win = newwin(list_height, list_width, start_y, start_x);
    scrollbar_win = newwin(list_height, 1, start_y, start_x + list_width - 1);
    if(brd_wnd == NULL || list_win == NULL || scrollbar_win == NULL)
        fatal("curses", "failed to create windows", CURSES_INIT_ERROR);

    box(brd_wnd, 0, 0);

    /* set up col names */
    wattron(brd_wnd, COLOR_PAIR(TERM_DEFAULT) | A_BOLD | A_REVERSE);
    mvwprintw(brd_wnd, 0, 2, "Name");
    if(show_sizes)
        mvwprintw(brd_wnd, 0, 2 + FILENAME_FIXED_LENGTH + WINDOW_BAR_LENGTH + 2 + 3, "Size"); // 2 spaces between filename and percentage bar + 3 spaces between percentage bar and size
    if(show_items)
        mvwprintw(brd_wnd, 0, 2 + FILENAME_FIXED_LENGTH + WINDOW_BAR_LENGTH + 2 + 3 + SIZE_COL_LENGTH + 1, "Items");
    if(show_subdirs)
        mvwprintw(brd_wnd, 0, 2 + FILENAME_FIXED_LENGTH + WINDOW_BAR_LENGTH + 2 + 3 + SIZE_COL_LENGTH + 1 + ITEMS_COL_LENGTH + 1, "Subdirs");
    if(show_files)
        mvwprintw(brd_wnd, 0, 2 + FILENAME_FIXED_LENGTH + WINDOW_BAR_LENGTH + 2 + 3 + SIZE_COL_LENGTH + 1 + ITEMS_COL_LENGTH + 1 + SUBDIRS_COL_LENGTH + 1, "Files");
    wattroff(brd_wnd, COLOR_PAIR(TERM_DEFAULT) | A_BOLD | A_REVERSE);

    refresh();
    wrefresh(brd_wnd);

    return true;
}

/* user selection - from now on the selection follows this entry */
void select_row(int index) {
    selected = index;
    selected_node = tree_view_row(view, index)->node;
}

/* lays out the rows if asked, then draws the ones on screen and the status - takes the tree lock.
   returns true if a layout was made after the scan had finished, i.e. it is final */
bool redraw(path_tree *pt, bool relayout) {

    g_mutex_lock(&pt->tree_access_mutex);

    // read before laying out: finished is set after the last node was added
    bool scan_finished = g_atomic_int_get(&pt->finished);

    int rows;
    if(relayout) {
        tree_view_layout(view, pt->root);
        rows = (int)view->rows->len;

        // keep the selection on the entry the user picked if it is still visible, otherwise on the same row
        int index = selected_node != NULL ? tree_view_find(view, selected_node) : -1;
        selected = index >= 0 ? index : MAX(MIN(selected, rows - 1), 0);
    }
    rows = (int)view->rows->len;

    /* scroll just enough to show the selection, without empty space below the last row */
    if(selected < top_row)
        top_row = selected;
    if(selected > top_row + list_height - 1)
        top_row = selected - (list_height - 1);
    top_row = MAX(MIN(top_row, rows - list_height), 0);

    werase(list_win);
    for(int line = 0; line < list_height && top_row + line < rows; line++) {
        wmove(list_win, line, 0);
        draw_row(list_win, tree_view_row(view, top_row + line));
        if(top_row + line == selected)
            mvwchgat(list_win, line, 0, -1, A_BOLD | A_REVERSE, TERM_DEFAULT, NULL);
    }

    draw_status(pt->root, scan_finished);

    g_mutex_unlock(&pt->tree_access_mutex);

    draw_scrollbar();

    wnoutrefresh(brd_wnd);         // first, the windows below lie on top of it
    wnoutrefresh(list_win);
    wnoutrefresh(scrollbar_win);   // after list_win, it covers its last column
    doupdate();

    return relayout && scan_finished;
}

/* scan progress or totals on the bottom border - caller must hold the tree lock */
void draw_status(GNode *root, bool scan_finished) {

    int y = list_height + 1;
    mvwhline(brd_wnd, y, 1, ACS_HLINE, list_width);    // erase the previous text

    gchar *text;
    if(root == NULL) {
        text = g_strdup(scan_finished ? " cannot read path " : " scanning... ");
    }
    else {
        path_data *pd = (path_data *)root->data;
        gchar *size = get_pretty_size(pd->size);
        text = g_strdup_printf(" %s %d items, %s ", scan_finished ? "done:" : "scanning...", pd->num_files + pd->num_dirs, size);
        g_free(size);
    }

    mvwprintw(brd_wnd, y, 2, "%s", text);
    g_free(text);
}

void draw_scrollbar(void) {

    int rows = (int)view->rows->len;
    int bar_pos = 0;
    int bar_height = list_height;

    if(rows > list_height) {
        // Calculate the ratio of the visible area to the total content area
        double visible_ratio = (double)list_height / rows;
        // Calculate the height of the scrollbar - at least 1
        bar_height = MAX((int)(visible_ratio * list_height), 1);
        // caution max pos is height - bar_height
        bar_pos = MIN(list_height - bar_height, (int)ceil(top_row * visible_ratio));
    }

    for(int i = 0; i < list_height; i++) {
        bool on_bar = i >= bar_pos && i < bar_pos + bar_height;
        mvwaddch(scrollbar_win, i, 0, ' ' | (on_bar ? A_REVERSE : COLOR_PAIR(TERM_DEFAULT)));
    }
}

void* build_tree(struct build_tree_struct *args) {
    path_tree_build(args->pt, args->path, &args->cancel, args->threads, args->one_file_system);
    return (void *)NULL;
}

/* draws one row at the cursor position of win - caller must hold the tree lock */
void draw_row(WINDOW *win, const view_row *row) {

    path_data *node_data = (path_data *)row->node->data;
    path_data *parent_data = (path_data *)row->node->parent->data;

    // indentation stops growing once only MIN_FILENAME_LENGTH columns are left for the name, so the row never overflows
    int indent = MIN((row->depth - 1) * 2, FILENAME_FIXED_LENGTH - MIN_FILENAME_LENGTH);
    wprintw(win, "%*s", indent, "");

    if(node_data->is_directory) {
        wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
        wprintw(win, "+");
        wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
    } else {
        wprintw(win, " ");
    }

    gchar* padded_str = get_str_normalized_padded(node_data->filename, FILENAME_FIXED_LENGTH - indent);
    wprintw(win, "%s  ", padded_str);
    g_free(padded_str);

    if(show_perc_bar) {

        if(parent_data->size > 0) {

            float perc = (float)((float)node_data->size / (float)parent_data -> size);
            int progress = (int)floor((double)WINDOW_BAR_LENGTH * perc);
            char *pretty = get_pretty_size(node_data->size);

            /* max percentage length 7 chars - ex: {'1', '0', '0', '.', '0', '%', '\0'} */
            char perc_string[7];
            snprintf(perc_string, 7, "%.1f%%", parent_data->size > 0 ? perc * 100 : 0.0);

            int bar_part_before = (int)((WINDOW_BAR_LENGTH - 6) / 2);

            if(progress <= bar_part_before) {
                wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                wprintw(win, "%*s", progress, "");
                wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);

                wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                wprintw(win, "%*s", bar_part_before - progress , "");
                wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);

                wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                wprintw(win, "%6s", perc_string);
                wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);

                wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                wprintw(win, "%*s", bar_part_before , "");
                wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
            }
            else {

                wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                wprintw(win, "%*s", bar_part_before, "");
                wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);

                for(int step = bar_part_before + 1, index = 0; step <= (WINDOW_BAR_LENGTH * 3) / 5 && index <= 6; step++, index++) {
                    if(step <= progress) {
                        wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                        if(strlen(perc_string) == 5 && index == 0)
                            wprintw(win, "%c", ' ');
                        wprintw(win, "%c", perc_string[index]);
                        wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                    }
                    else {
                        wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                        if(strlen(perc_string) == 5 && index == 0)
                            wprintw(win, "%c", ' ');
                        wprintw(win, "%c", perc_string[index]);
                        wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                    }
                }

                wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                wprintw(win, "%*s", (int)fmax(progress - (bar_part_before + 6), 0) , "");
                wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                wattron(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                wprintw(win, "%*s", (int)fmin(WINDOW_BAR_LENGTH - progress, bar_part_before), "");
                wattroff(win, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
            }

            if(show_sizes) {
                wattron(win, A_BOLD);
                wprintw(win, "   %-9s ", pretty);
                wattroff(win, A_BOLD);
            }

            g_free(pretty);
        }
        else {
            wprintw(win, "   %-*s ", WINDOW_BAR_LENGTH + 9, " ");
        }
    }

    if(show_items)   wprintw(win, "%-*d ", ITEMS_COL_LENGTH,   node_data->num_dirs + node_data->num_files);
    if(show_subdirs) wprintw(win, "%-*d ", SUBDIRS_COL_LENGTH, node_data->num_dirs);
    if(show_files)   wprintw(win, "%-*d",  FILES_COL_LENGTH,   node_data->num_files);
}

void fatal(const char *lib, const char *msg, int error_code) {
    flushinp();
    endwin();
    fprintf(stderr, "%s: %s\n", lib, msg);
    exit(error_code);
}
