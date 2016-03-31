# nav [![](https://api.travis-ci.org/jollywho/nav.svg)](https://travis-ci.org/jollywho/nav)

nav is a (**work in progress**) hackable ncurses file manager, inspired by vim, [ranger](http://ranger.nongnu.org/), and [dvtm](http://www.brain-dump.org/projects/dvtm/).

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
* libtermkey
* libuv
* w3m (image support)

    cmake .
    make
    sudo make install

## Configuration

Copy the sample config from `/etc/nav/navrc_example` to `~/.navrc`.

## Contributing

...is welcome.

## Future

* parser remake for more advanced features
* command pipes
* 'datatable' buffers for aggregate content
* strong filter support
