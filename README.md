# nav [![](https://api.travis-ci.org/jollywho/nav.svg)](https://travis-ci.org/jollywho/nav)

nav is a hackable ncurses file manager, inspired by vim, [ranger](http://ranger.nongnu.org/), and [dvtm](http://www.brain-dump.org/projects/dvtm/).
nav is a **work in progress**.

### What it has

* filemanager buffers
* w3mimgdisplay buffers
* embedded dvtm terminal
* async event-driven core
* scripting language
* vim-style autocmds, marks, maps, syntax, colors
* file executor/opener with 'before/after' cmds
* [ctrlp](https://kien.github.io/ctrlp.vim/)-inspired autocomplete menu

## Installation

### Compiling from Source

Install dependencies:

* cmake
* ncurses
* pcre
* [libtermkey](http://www.leonerd.org.uk/code/libtermkey/)
* [libuv](https://github.com/libuv/libuv)
* [w3m](http://w3m.sourceforge.net/) (optional: image support)

```bash
$ cmake .
$ make
$ sudo make install
```

## Configuration

Copy the sample config from `/etc/nav/navrc_example` to `~/.navrc`.

## Image Display

With w3mimgdisplay installed
```vim
new fm
vnew img
pipe #BUFFER_ID
```

## Contributing

...is welcome.

## Future

* parser remake for more advanced features
* 'datatable' buffers for aggregate content
* strong filter support
* command pipes
