## runit-2.1.1 2012100101 cheese fork

Small improvements to the chpst(8) utility of the runit suite.

Functionality:

 * New `-N` option causes `-0`, `-1` and `-2` to reopen standard descriptors
   to /dev/null, before any chroot, rather than closing them.
 * New `-A` option sets an *alarm(2)*.
 * rlimit options now accept '=', just like softlimit.
 * `-2` now preserves previously swallowed error messages

Documentation:

 * usage() text displays all options, including previously undocumented
   rlimits `-r` and `-t`
 * man page and html document all options

## Also see

* [http://smarden.org/runit/](http://smarden.org/runit/)
