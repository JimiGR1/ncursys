CC = gcc
CFLAGS = -Wall -g
PKG_CONFIG = pkg-config
GLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags glib-2.0)
NCURSES_CFLAGS = $(shell $(PKG_CONFIG) --cflags ncursesw)
NCURSES_LIBS = $(shell $(PKG_CONFIG) --libs ncursesw)
GLIB_LIBS = $(shell $(PKG_CONFIG) --libs glib-2.0) $(NCURSES_LIBS) -ltinfow -lm

# Define the target executable
TARGET = ncursys
SRCS = main.c path_data.c print_node.c print_tree.c unique_int_store.c path_tree.c helpers.c
OBJS = $(SRCS:.c=.o)

# Install locations
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Default target
all: $(TARGET)

# Compile target
$(TARGET): $(OBJS)
        $(CC) $(CFLAGS) -o $@ $^ $(GLIB_LIBS)

# Compile source files into object files
%.o: %.c
        $(CC) $(CFLAGS) $(GLIB_CFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

# Install
install: $(TARGET)
        install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

# Uninstall
uninstall:
        rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# Clean up
clean:
        rm -f $(OBJS) $(TARGET)

.PHONY: all clean install uninstall