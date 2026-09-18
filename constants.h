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
 
 
#ifndef constants_h
#define constants_h

#define TERM_DEFAULT 1

/* maximum number of entries in directory - defines pad height */
#define MAX_FILESYSTEM_WINDOW_LINES 1000
/* screen refresh interval in microseconds */
#define REFRESH_INTERVAL 500000
/* print tree rebuild interval in milliseconds */
#define REBUILD_PRINT_TREE_INTERVAL 10
/* optimal pad width */
#define OPT_PAD_WIDTH 108
/* terminal width must be at least MIN_COLS */
#define MIN_COLS 76
/* show files column if terminal width greater or equal */
#define SHOW_FILES_COL_THRESHOLD 109
/* show subdirs column if terminal width greater or equal */
#define SHOW_SUBDIRS_COL_THRESHOLD 99
/* show items column if terminal width greater or equal */
#define SHOW_ITEMS_COL_THRESHOLD 89
/* show size column if terminal width greater or equal */
#define SHOW_SIZES_COL_THRESHOLD 79
/* show percentage bar column if terminal width greater or equal */
#define SHOW_PERC_BAR_COL_THRESHOLD 67
/* fixed filename length */
#define FILENAME_FIXED_LENGTH 30
/* progress bar length */
#define WINDOW_BAR_LENGTH 30
/* size col length */
#define SIZE_COL_LENGTH 9
/* items col length */
#define ITEMS_COL_LENGTH 9
/* subdirs col length */
#define SUBDIRS_COL_LENGTH 9
/* files col length */
#define FILES_COL_LENGTH 9

/* since no header is used */
#define HEADER_HEIGHT 0

#endif /* constants_h */
