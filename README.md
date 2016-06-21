# nav [![](https://api.travis-ci.org/jollywho/nav.svg)](https://travis-ci.org/jollywho/nav)
nav is a hackable ncurses file manager, inspired by vim, [ranger](http://ranger.nongnu.org/), and [dvtm](http://www.brain-dump.org/projects/dvtm/).

nav is a **work in progress**.

![](http://sicp.me/u/armtv.png)

-----------------
- [Installation](#installation)
- [Image Display](#image-display)
- [Clipboard](#clipboard)
- [Default Bindings](#default-bindings)
- [Expansion Symbols](#expansion-symbols)
- [Non Goals](#non-goals)
- [Future](#future)
- [Contributing](#contributing)

### What it has

* filemanager buffers
* w3mimgdisplay buffers
* embedded dvtm terminal
* async event-driven core
* simple script language (Navscript)
* vim-style autocmds, marks, maps, syntax, colors
* multi-selection: move, remove, copy
* bulkrename
* directory jumplist
* file executor/opener with 'before/after' cmds.
* strong [ctrlp](http://kien.github.io/ctrlp.vim/)-inspired autocomplete menu
* menu hint keys

## Installation

### Compiling from Source

Install dependencies:

* cmake
* ncurses
* pcre
* [libtermkey](http://www.leonerd.org.uk/code/libtermkey/)
* [libuv](http://github.com/libuv/libuv)
* [w3m](http://w3m.sourceforge.net/) (optional: image support)

```bash
$ cmake .
$ make
$ sudo make install
```

## Image Display

With w3mimgdisplay installed
```viml
:new fm
:vnew img
:di #BUFFER_ID
```
```viml
:vnew img #BUFFER_ID
```

## Clipboard

```
:set copy-pipe #SELECT_PROGRAM
```
example:
```
:set copy-pipe xclip -i
```
`

## Default Bindings

See [Wiki](https://github.com/jollywho/nav/wiki#default-mappings)

## Expansion Symbols

See [Wiki](https://github.com/jollywho/nav/wiki#expansion-symbols)

## Non Goals

* text editor
* making Navscript into a full-featured language

## Future

* parser remake for more advanced features
* 'datatable' buffers for aggregate content
* command pipes

## Contributing

...is much welcome.
