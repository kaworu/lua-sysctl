# sysctl(3) C interface for Lua

lua-sysctl aim provide a simple and complete interface to FreeBSD's `sysctl(3)` C function for the [Lua](http://lua.org) scripting language. It allow both reading and writing sysctl values (see [limitations](#limitations)).

Although the project has been started to display system informations inside the [Awesome window manager](http://awesome.naquadah.org/) it has been designed as a general purpose interface, allowing it to be embeded into bigger project, used as library by system administration scripts etc.

Most of the implementation is based on FreeBSD's `sysctl(8)` command line tool. For more informations about sysctl see:
* http://www.freebsd.org/doc/handbook/configtuning-sysctl.html
* http://www.freebsd.org/cgi/man.cgi?query=sysctl&amp;sektion=8
* http://www.freebsd.org/cgi/man.cgi?query=sysctl&amp;sektion=3

## Installation

Thanks to garga@ **lua-sysctl** is in the port tree under _devel/lua-sysctl_.

## Examples

TODO ! :)

## Limitations

* Some sysctl variablse cannot be changed while the system is running (they're "read-only tunable").
* You need root privilege to change sysctl variables
* Some variables cannot be changed from inside a jail (and might depend of the securelevel too).

Theses limitations are not due to the implementation of **lua-sysctl** but rather to `sysctl(3)` and how FreeBSD work. Note that most (if not all) theses limitations are desirables.

* **lua-sysctl** is not able to handle values for all existing types. More precisely, it can handle the same subset supported by the `sysctl(8)` command line utility. It should not be an issue since most sysctl key have "simple" values (numeric or string).
