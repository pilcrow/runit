## runit-2.1.1 cheese fork (2012102301)

Small improvements to the *chpst(8)* utility of the runit suite.

Functionality:

 * New `-N` option causes `-0`, `-1` and `-2` to reopen standard descriptors
   to /dev/null, before any chroot, rather than closing them.
 * `-/ +root` works just like `-/ root`, but account lookups (`-u`/`-U`) and
   lockfiles (`-l`/`-L`) are performed before *chroot(2)*.
 * `-x` option exits successfully and silently if file locking fails, like
   *setlock*.
 * New `-A` option sets or clears an *alarm(2)*.
 * Soft resource limit options now accept '=', just like *softlimit*.
 * `-2` now preserves previously swallowed error messages

Documentation:

 * usage() text displays all options, including previously undocumented
   rlimits `-r` and `-t`
 * man page and html document all options

## Also see

* [http://smarden.org/runit/](http://smarden.org/runit/)
