# ncursys

A terminal-based tool writen in curses to visualize hard disk usage.

## Dependencies

- [glib](https://gitlab.gnome.org/GNOME/glib) (>= 2.x)
- ncurses, wide-character build (`ncursesw`)
- A C compiler (gcc or clang)
- pkg-config

### Installing dependencies

**Debian / Ubuntu:**

    sudo apt install build-essential pkg-config libglib2.0-dev libncursesw5-dev

**Gentoo:**

    sudo emerge dev-libs/glib sys-libs/ncurses virtual/pkgconfig

**Fedora:**

    sudo dnf install gcc make pkgconf-pkg-config glib2-devel ncurses-devel

## Building from source

    make

This produces a `ncursys` binary in the project root.

## Installing

### From source

    sudo make install

(if you add an `install` target — see note below)

### Gentoo

Available via [GURU](https://github.com/gentoo/guru):

    sudo eselect repository enable guru
    sudo emerge --sync guru
    sudo emerge app-misc/ncursys

## Usage

    ncursys <path> [--threads=N] [-x]
	'q' to exit

### Arguments

| Argument           | Required | Description                                        |
|--------------------|----------|----------------------------------------------------|
| `<path>`           | Yes      | The directory path to scan/process.                |
| `--threads=N`      | No       | Number of worker threads to use (default: CPU + 2).      |
| `-x`, `--one-file-system` | No | Skip directories on other filesystems, like `du -x`. |

Sizes are computed like `du`: allocated blocks are counted, directories included, and hard links are counted once.

### Examples

    sudo ncursys /

Scan only the root filesystem (skips `/proc`, `/sys` and other mounts):

    sudo ncursys -x /

Run with a custom thread count:

    ncursys /home/user --threads=8

## License

GPL-3.0-or-later — see [LICENSE](LICENSE) for the full text.

## Author

Dimitris Trivizakis — dim.trivizakis@yandex.com