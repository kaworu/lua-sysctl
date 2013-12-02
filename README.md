# sysctl(3) C interface for Lua

lua-sysctl aim provide a simple and complete interface to FreeBSD's `sysctl(3)`
C function for the [Lua](http://lua.org) scripting language. It allow both
reading and writing sysctl values (see [limitations](#limitations)).

Although the project has been started to display system informations inside the
[Awesome window manager](http://awesome.naquadah.org/) it has been designed as
a general purpose interface, allowing it to be embeded into bigger project,
used as library by system administration scripts etc.

Most of the implementation is based on FreeBSD's `sysctl(8)` command line tool.
For more informations about sysctl see:

* http://www.freebsd.org/doc/handbook/configtuning-sysctl.html
* http://www.freebsd.org/cgi/man.cgi?query=sysctl&amp;sektion=8
* http://www.freebsd.org/cgi/man.cgi?query=sysctl&amp;sektion=3

## Installation

Thanks to garga@ **lua-sysctl** is in the port tree under _devel/lua-sysctl_.

## Examples

Reading:
```
> require('sysctl')
> val, type = sysctl.get('kern.ostype') -- reading a string
> print(val)
FreeBSD
> print(type)
A
> val, type = sysctl.get('kern.maxvnodes') -- reading a integer value
> print(val)
111376
> print(type)
I
> table, type = sysctl.get('vm.vmtotal') -- reading a special type value (which will be a table in Lua)
> print(table)
table: 0x801415800
> print(type)
S,vmtotal
> for k,v in pairs(table) do print(k,v) end
sl  20
rm  81264
avmshr  6884
dw  0
free    1601444
pw  0
armshr  6252
vmshr   22048
rmshr   7204
arm 35104
rq  1
vm  1074232888
avm 420396
```

Writting:
```
> require('sysctl')
> sysctl.set('security.bsd.see_other_uids', 0)
```

## Limitations

* Some sysctl variables cannot be changed while the system is running (they're
  "read-only tunable").
* You need root privilege to change sysctl variables
* Some variables cannot be changed from inside a jail (and might depend of the
  securelevel too).

Theses limitations are not due to the implementation of **lua-sysctl** but
rather to `sysctl(3)` and how FreeBSD work. Note that most (if not all) theses
limitations are desirables.

* **lua-sysctl** is not able to handle values for all existing types. More
  precisely, it can handle the same subset supported by the `sysctl(8)` command
  line utility. It should not be an issue since most sysctl key have "simple"
  values (numeric or string).
