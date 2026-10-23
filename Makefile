CC ?= gcc
CFLAGS ?= -Wall -g
PKG_CONFIG ?= pkg-config
GLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags glib-2.0)
NCURSES_CFLAGS = $(shell $(PKG_CONFIG) --cflags ncursesw)
NCURSES_LIBS = $(shell $(PKG_CONFIG) --libs ncursesw)
GLIB_LIBS = $(shell $(PKG_CONFIG) --libs glib-2.0) $(NCURSES_LIBS) -lm

# Define the target executable
TARGET = ncursys
SRCS = main.c path_data.c path_tree.c tree_view.c helpers.c
OBJS = $(SRCS:.c=.o)

# Install locations
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Default target
all: $(TARGET)

# Compile target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(GLIB_LIBS)

# Compile source files into object files
# -MMD -MP also write the headers each object depends on into a .d file, so editing a header rebuilds its users
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP $(GLIB_CFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

-include $(OBJS:.o=.d)

# Install
install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

# Uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# Clean up
clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET)

.PHONY: all clean install uninstall