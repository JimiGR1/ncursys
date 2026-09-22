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
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <ncurses.h>
#include <math.h>
#include <assert.h>
#include <locale.h>
#include <signal.h>
#include <getopt.h>
#include "path_data.h"
#include "path_tree.h"
#include "print_node.h"
#include "print_tree.h"
#include "errors.h"
#include "constants.h"
#include "helpers.h"

// this is passed to build_tree_thread
struct build_tree_struct {
    path_tree **pt;
    volatile int thread_state;
    const char *path;
    int threads;
};

// this is passed to update_size_thread
struct size_tree_struct {
    path_tree **pt;
    volatile int thread_state;
};

void*    build_tree(struct build_tree_struct *args);
void*    get_tree_size(struct size_tree_struct *args);

void     error(char *lib, char *msg, int error_code);
void     init_curses(void);
void     handle_resize(int sig);

bool     add_in_print_tree_root (GNode *node, gpointer data);
void     add_in_print_tree_parent (GNode *node, GNode *parent, print_tree** p_tree);
gboolean print_node_data (GNode *node, gpointer data);

/* variables */

WINDOW  *brd_wnd,                           /* this refers to the top left boxed window */
        *fstm_window,                       /* this refers to the pad containing the main output */
        *scrollbar_win;                     /* this refers to the vertical scrollbar on the rightmost columns of brd_wnd */

int     fstm_pad_height,                    /* this is the number of rows the pad can display in screen */
        fstm_pad_width  = OPT_PAD_WIDTH;    /* this is the number of columns the pad displays: this is fixed we do not pad left - right */

int     start_y = 1,                        /* this is the offset from world 0 row to brd_wnd zero row */
        start_x = 1,                        /* this is the offset from world 0 col to brd_wnd zero col */
        pad_pos = 0;                        /* this is the current start position */

volatile sig_atomic_t caught_resize = 0;    /* this becomes 1 when resize is raised (SIGWINCH caught) */

bool    show_perc_bar   = true,             /* this is true if width can hold perc bar number */
        show_sizes      = true,             /* this is true if width can hold size number */
        show_items      = true,             /* this is true if width can hold item number */
        show_subdirs    = true,             /* this is true if width can hold subdir number */
        show_files      = true;             /* this is true if width can hold file number */
        
int     total_fst_window_lines = 0;         /* this holds the total number of lines in brd_wnd  */

int main(int argc, char * argv[]) {
    
    // Set the locale to follow shell env
    setlocale(LC_ALL, "");
    // Define long options
    static struct option long_options[] = { {"threads", required_argument, 0, 't'}, {0, 0, 0, 0} };
    
    int opt, option_index, thread_number = g_get_num_processors() + 2;
    char *path = NULL;

    while ((opt = getopt_long(argc, argv, "t:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 't':
                thread_number = atoi(optarg);
                break;
            case '?':
                fprintf(stderr, "Unknown option or missing argument. Usage: %s path [--threads thread_number]\n", argv[0]);
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "Usage: %s path [--threads n_threads]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }
    
    // Remaining argument is the path
    if (optind < argc) {
        path = argv[optind];
    } else {
        fprintf(stderr, "Usage: %s path [--threads thread_number]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    init_curses();
    
    if (COLS <= fstm_pad_width) {
        fstm_pad_width = COLS - 2;
    }
    
    fstm_pad_height = LINES - HEADER_HEIGHT - 2;
    
    show_files = COLS >= SHOW_FILES_COL_THRESHOLD;
    show_subdirs = COLS >= SHOW_SUBDIRS_COL_THRESHOLD;
    show_items = COLS >= SHOW_ITEMS_COL_THRESHOLD;
    show_sizes = COLS >= SHOW_SIZES_COL_THRESHOLD;
    show_perc_bar = COLS >= SHOW_PERC_BAR_COL_THRESHOLD;


    brd_wnd = newwin(fstm_pad_height + 2, fstm_pad_width + 2, start_y - 1, start_x - 1);
    box(brd_wnd, 0, 0);
    fstm_window = newpad(MAX_FILESYSTEM_WINDOW_LINES, fstm_pad_width);
    scrollbar_win = newwin(fstm_pad_height, 1, start_y, start_x + fstm_pad_width - 1);
    
    int curs_pos = start_y;
    int scrollbar_pos = 0;
    int scrollbar_height = 0;
    double visible_pad_ratio = 1.0;
    
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
        
    prefresh(fstm_window, pad_pos, 0, start_y, start_x, start_y + fstm_pad_height - 1, start_x + fstm_pad_width - 1);
    move(start_y, start_x); // Move the cursor
    
    //refresh();
    
    GThread *build_tree_thread, *update_size_thread;
    path_tree *pt = path_tree_init();
    if(pt == NULL) {
        error("main", "path_tree creation failed memory allocation", MEMORY_ALLOCATION_ERROR);
    }
    
    print_tree *p_tree = print_tree_init();
    if(p_tree == NULL) {
        error("main", "print_tree creation failed memory allocation", MEMORY_ALLOCATION_ERROR);
    }
    
    int thread_state = 1;
    struct build_tree_struct build_thread_args = {&pt, thread_state, path, thread_number};
    build_tree_thread = g_thread_new("build_tree_thread", (GThreadFunc)build_tree, &build_thread_args);
    
    int thread2_state = 1;
    struct size_tree_struct size_thread_args = {&pt, thread2_state};
    update_size_thread = g_thread_new("update_size_thread", (GThreadFunc)get_tree_size, &size_thread_args);
    

    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    // update print tree layout every
    // does not apply for user input - these apply instantly
    const int delay = REFRESH_INTERVAL; // delay in microseconds
    
    int ch;
    bool exit = false;
    while(!exit) {
        ch = getch();
        if(ch != ERR) { // key is pressed
            switch(ch) {
                case 'q':
                    exit = true;
                    break;
                case KEY_UP: {
                    // if cursor reached start of pad and more output lies above: scroll up
                    if(curs_pos == start_y && pad_pos > 0) {
                        mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_NORMAL, TERM_DEFAULT, NULL);
                        pad_pos--;
                        mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_BOLD | A_REVERSE, TERM_DEFAULT, NULL);
                    }
                    // else just move cursor up
                    else if(curs_pos > start_y) {
                        mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_NORMAL, TERM_DEFAULT, NULL);
                        curs_pos--;
                        mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_BOLD | A_REVERSE, TERM_DEFAULT, NULL);
                    }
                    break;
                }
                case KEY_DOWN: {
                    // if cursor reached end of pad and more output lies below: scroll down
                    if(curs_pos == start_y + fstm_pad_height - 1 && pad_pos < total_fst_window_lines - fstm_pad_height) {
                        mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_NORMAL, TERM_DEFAULT, NULL);
                        pad_pos++;
                        mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_BOLD | A_REVERSE, TERM_DEFAULT, NULL);
                    }
                    // else just move cursor down
                    else if(curs_pos < start_y + fstm_pad_height - 1) {
                        if(curs_pos - start_y + 1 < total_fst_window_lines) {
                            mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_NORMAL, TERM_DEFAULT, NULL);
                            curs_pos++;
                            mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_BOLD | A_REVERSE, TERM_DEFAULT, NULL);
                        }
                    }
                    break;
                }
                case 'i': {
                    // TODO: handle print info popup-window
                    break;
                }
                case '\n': {

                    int entry_index = curs_pos + pad_pos - start_y; // this is the real vertical index of selected entry [0, EntriesNumber - 1]
                    
                    entry_index++; // in position zero is root so inc
                    
                    // Get corresponding print tree node

                    if(p_tree->root == NULL)
                        continue;

                    GNode *fpn0 = print_tree_get_tree_node_by_v_index(p_tree, &entry_index);
                    
                    print_node * fpn = (print_node *)fpn0->data;
                    
                    // get corresponding path_data from path_tree
                    path_data * pdn = (path_data *)(fpn->node)->data;
                    
                    // sanity check
                    assert(pdn != NULL);
                    
                    if(pdn->is_directory) {
                        
                        if(fpn->node_state == COLLAPSED) {
                            fpn->node_state = EXPANDED;
                            
                            // lock path tree since we will access it
                            g_mutex_lock(&(pt->tree_access_mutex));
                            
                            // get pointer in corresponding path node
                            GNode *path_tree_node = fpn->node;
                            // iterate his children and add to print tree
                            for (GNode *child = path_tree_node->children; child != NULL; child = child->next) {
                                path_data *cpd = (path_data *)child->data;
                                assert(cpd != NULL);
                                add_in_print_tree_parent(child, fpn0, &p_tree);
                            }
                            
                            // unlock path tree
                            g_mutex_unlock(&(pt->tree_access_mutex));
                        }
                        else {

                            // get descendants num (not only first level is retrieved)
                            if(p_tree->root != NULL) {
                                int n = print_tree_n_descendants(fpn0);                            
                                for(int i = 0; i < n; i++) {
                                    wmove(fstm_window, curs_pos + pad_pos + i, 0);
                                    wclrtoeol(fstm_window);
                                }
                            }
                            
                            print_tree_remove_descendants(fpn0);
                            assert(fpn0->children == NULL);
                            fpn->node_state = COLLAPSED; 
                            
                        }
                        
                    } else {
                        // TODO; maybe show a popup for non directory entries with more details would be nice
                        
                    }
                    
                    break;
                    
                }
                default:
                    break;
                    
            }
            
            // Calculate the ratio of the visible area to the total content area
            visible_pad_ratio = (double)fstm_pad_height / total_fst_window_lines;
            // Calculate the height of the scrollbar - at least 1
            scrollbar_height = fmax(visible_pad_ratio * fstm_pad_height, 1);
            
            if(fstm_pad_height > total_fst_window_lines)
                scrollbar_height = fstm_pad_height;
            
            // caution max pos is height - scrollbar_height
            scrollbar_pos = fmin(fstm_pad_height - scrollbar_height, ceil((pad_pos) * visible_pad_ratio));
            for(int i = 0; i < scrollbar_pos; i++)
                mvwaddch(scrollbar_win, i, 0, ' ' | COLOR_PAIR(TERM_DEFAULT));
            
            for(int i = 0; i < scrollbar_height; i++)
                mvwaddch(scrollbar_win, scrollbar_pos + i, 0, ' ' | A_REVERSE);
            
            for(int i = scrollbar_pos + scrollbar_height; i < fstm_pad_height; i++)
                mvwaddch(scrollbar_win, i, 0, ' ' | COLOR_PAIR(TERM_DEFAULT));
            
            wmove(fstm_window, 0, 0);
            pnoutrefresh(fstm_window, pad_pos, 0, start_y, start_x, start_y + fstm_pad_height - 1, start_x + fstm_pad_width - 1);
            wnoutrefresh(scrollbar_win);

            move(curs_pos, start_x); // Move the cursor TODO CHECK IF NEEDED
            
            doupdate();
            gettimeofday(&start_time, NULL); // Reset the start time
            
            
        }
        else {
            gettimeofday(&current_time, NULL);
            long elapsed_time = (current_time.tv_sec - start_time.tv_sec) * 1000000L + (current_time.tv_usec - start_time.tv_usec);
            
            if (elapsed_time >= delay) {
                
                g_mutex_lock(&pt->tree_access_mutex);
                                                
                // if path_tree is ready
                if(pt != (path_tree *)NULL && pt->root != (GNode *)NULL) {
                    // if print_tree root not added (FIRST TIME ONLY)
                    if(p_tree != (print_tree *)NULL && p_tree->root == (GNode *)NULL) {
                        print_node *new_print_node = print_node_create(pt->root);
                        if(new_print_node == NULL) {
                            error("print_tree_loop", "failed to allocate new print_node", MEMORY_ALLOCATION_ERROR);
                        }
                        print_tree_append(&p_tree, NULL, new_print_node);
                    }
                    // fill print tree - nodes already added are omitted
                    // right now only first level is added
                    g_node_children_foreach (pt->root, G_TRAVERSE_ALL, (void (*)(GNode*, gpointer))add_in_print_tree_root , &p_tree);
                    
                }
                
                // traverse tree - flatten - print
                if(p_tree != (print_tree *)NULL && p_tree->root != (GNode *)NULL) {
                    total_fst_window_lines = g_node_n_nodes(p_tree->root, G_TRAVERSE_ALL) - 1;
                    wmove(fstm_window, 0, 0);
                    print_tree_sort_children(p_tree->root);
                    int v_index = 1;
                    g_node_traverse (p_tree->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, (GNodeTraverseFunc)print_node_data, &v_index);
                    /* if total rows are less than pad height clear everything above last row from leftovers (usefull after collapse) */
                    if(total_fst_window_lines < fstm_pad_height) {
                        for(int i = 0; i < fstm_pad_height - total_fst_window_lines; i++) {
                            wmove(fstm_window, total_fst_window_lines + i, 0);
                            wclrtoeol(fstm_window);
                        }
                    }
                }
                
                g_mutex_unlock(&pt->tree_access_mutex);
                
                wmove(fstm_window, 0, 0);
                mvwchgat(fstm_window, pad_pos + (curs_pos - start_y), 0, -1, A_BOLD | A_REVERSE, TERM_DEFAULT, NULL);
                pnoutrefresh(fstm_window, pad_pos, 0, start_y, start_x, start_y + fstm_pad_height - 1, start_x + fstm_pad_width - 1);
                wnoutrefresh(scrollbar_win);
                move(curs_pos, start_x); // Move the cursor TODO CHECK IF NEEDED
                //refresh();
                doupdate();
                gettimeofday(&start_time, NULL); // Reset the start time
                
            }
            
            /* redundant after 1st run - isn't it? */
            if(total_fst_window_lines > 0) {
                // Calculate the ratio of the visible area to the total content area
                visible_pad_ratio = (double)fstm_pad_height / total_fst_window_lines;
                // Calculate the height of the scrollbar - at least 1
                scrollbar_height = fmax(visible_pad_ratio * fstm_pad_height, 1);
                
                if(fstm_pad_height > total_fst_window_lines)
                    scrollbar_height = fstm_pad_height;
                
                scrollbar_pos = fmin(fstm_pad_height - scrollbar_height, ceil((pad_pos) * visible_pad_ratio));
                for(int i = 0; i < scrollbar_pos; i++)
                    mvwaddch(scrollbar_win, i, 0, ' ' | COLOR_PAIR(TERM_DEFAULT));
                
                for(int i = 0; i < scrollbar_height; i++)
                    mvwaddch(scrollbar_win, scrollbar_pos + i, 0, ' ' | A_REVERSE);
                
                for(int i = scrollbar_pos + scrollbar_height; i < fstm_pad_height; i++)
                    mvwaddch(scrollbar_win, i, 0, ' ' | COLOR_PAIR(TERM_DEFAULT));
                
                wrefresh(scrollbar_win);
            }
            
         
            // sleep ms to reduce CPU usage
            napms(REBUILD_PRINT_TREE_INTERVAL);
            curs_set(0); // hide main curser
        }
        
        // check if SIGWINCH was raised - but do whatever here in main thread
        if(caught_resize) {
            
            /* reset the terminal to its normal operating mode, which can help clear any artifacts or issues caused by the window resize. */
            endwin();
            clear();
            refresh();
            resizeterm(LINES, COLS);
                
            if (COLS <= OPT_PAD_WIDTH) {
                fstm_pad_width = COLS - 2;
            }
            
            fstm_pad_height = LINES - HEADER_HEIGHT - 2;
            
            show_files = COLS >= SHOW_FILES_COL_THRESHOLD;
            show_subdirs = COLS >= SHOW_SUBDIRS_COL_THRESHOLD;
            show_items = COLS >= SHOW_ITEMS_COL_THRESHOLD;
            show_sizes = COLS >= SHOW_SIZES_COL_THRESHOLD;
            show_perc_bar = COLS >= SHOW_PERC_BAR_COL_THRESHOLD;
            
            /* brd_wnd */
            wresize(brd_wnd, fstm_pad_height + 2, fstm_pad_width + 2);
            wmove(brd_wnd, start_y - 1, start_x - 1);
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
                        
            /* fstm_window */
            wresize(fstm_window, MAX_FILESYSTEM_WINDOW_LINES, fstm_pad_width);
            mvwin(scrollbar_win, start_y, start_x + fstm_pad_width - 1);
            
            wrefresh(scrollbar_win);
            refresh();
            wrefresh(brd_wnd);
            
            caught_resize = 0;
            
        }
        
    }
    
    // clear curses to restore terminal
    flushinp();
    endwin();
    
    printf("Finishing...\n");
    
    build_thread_args.thread_state = 0;
    size_thread_args.thread_state = 0;
    
    // Wait for the threads to terminate
    g_thread_join(build_tree_thread);
    g_thread_join(update_size_thread);
    
    // cleanup
    //print_tree_destroy(p_tree);
    path_tree_destroy(pt);
    
    return 0;
}

void init_curses(void) {
    initscr();
    
    if(has_colors()) {
        use_default_colors();
        if(start_color() != OK)
            error("curses", "Terminal cannot start colors", CURSES_INIT_ERROR);
    }
    
    init_pair(TERM_DEFAULT, -1, -1);
    
    signal(SIGWINCH, handle_resize); // Set up the signal handler
    noecho();                        // Don't echo any keypresses
    cbreak();                        // Don't wait for enter; pass back every keypress
    nodelay(stdscr, true);           // set non blocking
    keypad(stdscr, true);            // Enable arrow keys
    // Enable mouse events
    //mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
}

void handle_resize(int sig) {
    caught_resize = true;
}

void* get_tree_size(struct size_tree_struct *args) {
    for(;;) {
        if(args->thread_state == 0)
            break;
        /* run tree leaves iteration for sizes update every 900ms to reduce CPU cycles */
        napms(900);
        path_tree_update_sizes(args->pt);
        if((*(args->pt))->finished) {
            path_tree_update_sizes(args->pt);
            break;
        }
    }
    return (void *)NULL;
}

void* build_tree(struct build_tree_struct *args) {
    path_tree_build(args->pt, args->path, &args->thread_state, args->threads);
    return (void *)NULL;
}

bool add_in_print_tree_root (GNode *node, gpointer data) {
    print_tree **p_tree = (print_tree **)data;
    print_node *new_print_node = print_node_create(node);
    if(new_print_node == NULL) {
        error("func[add_in_print_tree_root]", "failed to allocate new print_node", MEMORY_ALLOCATION_ERROR);
    }
    if(print_tree_append(p_tree, (*p_tree)->root, new_print_node) == (GNode *)NULL) { // if exists - free memory and skip
        g_free(new_print_node);
        new_print_node = (print_node *)NULL;
    }
    
    return false;
}

void add_in_print_tree_parent (GNode *node, GNode *parent, print_tree** p_tree) {
    print_node *new_print_node = print_node_create(node);
    if(new_print_node == NULL) {
        error("func[add_in_print_tree_parent]", "failed to allocate new print_node", MEMORY_ALLOCATION_ERROR);
    }
    if(print_tree_append(p_tree, parent, new_print_node) == (GNode *)NULL) {
        g_free(new_print_node);
        new_print_node = (print_node *)NULL;
    }
}

gboolean print_node_data (GNode *node, gpointer data) {
    
    int *counter = (int *)data;
    
    print_node *pn = (print_node *)node->data;
    GNode *tn = pn->node;
    path_data *node_data = (path_data *)tn->data;
    
    if(G_NODE_IS_ROOT(node))
        return false;
    
    pn->v_index = *counter;
    *counter += 1;
    
    pn->level = g_node_depth(node);
    
    GNode *parent_node = tn->parent;
    path_data *parent_data = (path_data *)parent_node ->data;
    
    wprintw(fstm_window, "%*s", (pn->level - 2) * 2 , "");
    
    if(node_data->is_directory) {
        wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
        wprintw(fstm_window, "+");
        wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
    } else {
        wprintw(fstm_window, " ");
    }
    
    gchar* padded_str = get_str_normalized_padded(node_data->filename, pn->level, FILENAME_FIXED_LENGTH);
    wprintw(fstm_window, "%s  ", padded_str);
    g_free(padded_str);
    
    if(show_perc_bar) {
        
        if(parent_data->size > 0) {
            
            float perc = (float)((float)node_data->size / (float)parent_data -> size);
            int progress = (int)floor((double)WINDOW_BAR_LENGTH * perc);
            char *pretty = get_pretty_size(node_data->size);
            if(pretty == NULL)
                error("main", "get_pretty_size allocation failed failed", MEMORY_ALLOCATION_ERROR);
            
            /* max percentage length 7 chars - ex: {'1', '0', '0', '.', '0', '%', '\0'} */
            char perc_string[7];
            snprintf(perc_string, 7, "%.1f%%", parent_data->size > 0 ? perc * 100 : 0.0);
            
            int bar_part_before = (int)((WINDOW_BAR_LENGTH - 6) / 2);
            
            if(progress <= bar_part_before) {
                wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                wprintw(fstm_window, "%*s", progress, "");
                wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                
                wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                wprintw(fstm_window, "%*s", bar_part_before - progress , "");
                wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                
                wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                wprintw(fstm_window, "%6s", perc_string);
                wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                
                wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                wprintw(fstm_window, "%*s", bar_part_before , "");
                wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
            }
            else {
                
                wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                wprintw(fstm_window, "%*s", bar_part_before, "");
                wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                
                for(int step = bar_part_before + 1, index = 0; step <= (WINDOW_BAR_LENGTH * 3) / 5 && index <= 6; step++, index++) {
                    if(step <= progress) {
                        wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                        if(strlen(perc_string) == 5 && index == 0)
                            wprintw(fstm_window, "%c", ' ');
                        wprintw(fstm_window, "%c", perc_string[index]);
                        wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                    }
                    else {
                        wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                        if(strlen(perc_string) == 5 && index == 0)
                            wprintw(fstm_window, "%c", ' ');
                        wprintw(fstm_window, "%c", perc_string[index]);
                        wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                    }
                }
                
                wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                wprintw(fstm_window, "%*s", (int)fmax(progress - (bar_part_before + 6), 0) , "");
                wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE | A_BOLD);
                wattron(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
                wprintw(fstm_window, "%*s", (int)fmin(WINDOW_BAR_LENGTH - progress, bar_part_before), "");
                wattroff(fstm_window, COLOR_PAIR(TERM_DEFAULT) | A_REVERSE);
            }
            
            if(show_sizes) {
                wattron(fstm_window, A_BOLD);
                wprintw(fstm_window, "   %-9s ", pretty);
                wattroff(fstm_window, A_BOLD);
            }
            
            g_free(pretty);
        }
        else {
            wprintw(fstm_window, "   %-*s ", WINDOW_BAR_LENGTH + 9, " ");
        }
    }
    
    if(show_items)   wprintw(fstm_window, "%-*d ", ITEMS_COL_LENGTH,   node_data->num_dirs + node_data->num_files);
    if(show_subdirs) wprintw(fstm_window, "%-*d ", SUBDIRS_COL_LENGTH, node_data->num_dirs);
    if(show_files)   wprintw(fstm_window, "%-*d",  FILES_COL_LENGTH,   node_data->num_files);
    
    wprintw(fstm_window, "\n");
    
    return false;
}

void error(char *lib, char *msg, int error_code) {
    flushinp();
    endwin();
    printf("%s: %s\n", lib, msg);
    exit(error_code);
}



