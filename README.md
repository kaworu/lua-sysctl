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

### Using the FreeBSD ports tree
Thanks to garga@FreeBSD and @uzsolt, lua-sysctl is in the FreeBSD ports
tree under _devel/lua-sysctl_.

NOTE: development is done on `master` branch, look for the branch matching your
lua version if you want to build.

## Examples

Reading:
```
> sysctl = require('sysctl')
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
> sysctl = require('sysctl')
> sysctl.set('security.bsd.see_other_uids', 0)
```

## Functions

NOTE: Both sysctl.get() and sysctl.set() raise an error if any problem occur.
If you don't control the key you're passing to these function you might want to
use lua's protected calls (pcall).

### sysctl.get(key)
Rreturns two values: The first returned value is the sysctl(3) value, the
second value is the format.

#### formats
- _I_ `int`
- _UI_ `unsigned int`
- _IK_ `int`, in (kelv * 10) (used to get temperature)
- _L_ `long`
- _UL_ `unsigned long`
- _A_ `char *`
- _S,clockinfo_ `struct clockinfo`
- _S,loadavg_ `struct loadavg`
- _S,timeval_ `struct timeval`
- _S,vmtotal_ `struct vmtotal`

In lua land, it means that:
- _I_, _UI_, _IK_, _L_, _UL_, are numbers.
- _A_ is a string.
- _S,clockinfo_ is a table of integers
  `{ hz, tick, profhz, stathz }`
- _S,loadavg_ is an array of numbers
  `{ 1, 2, 3 }`
- _S,timeval_ is a table of integers
  `{ sec, sec }`
- _S,vmtotal_ is a table of integers
  `{ rq, dw, pw, sl, vm, avm, rm, arm, vmshr, avmshr, rmshr, armshr, free }`

### sysctl.set(key, newval)
Set the sysctl's key to newval. Return nothing and throw lua error if any
problem occur. Note that some sysctl's key are read only or read only tunable
and can not be set at runtime.

### sysctl.IK2celsius(kelv)
Convert a sysctl's IK value into celsius and return it.

### sysctl.IK2farenheit(kelv)
convert a sysctl's IK value into farenheit and return it.

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
