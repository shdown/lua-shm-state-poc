PoC of sharing Lua interpreter memory between multiple processes.

It features two processes that take turns calling functions `f()` and `g()` *in the same Lua state* (loaded from `demo.lua`).

![Screenshot](https://user-images.githubusercontent.com/5462697/41799127-e976f7a6-7678-11e8-8b06-fb6b4ee1d005.png)

How it Works
===
There’s nothing new in Lua being flexible. It allows to override all memory management by passing a custom allocator to [`lua_newstate`](https://www.lua.org/manual/5.3/manual.html#lua_newstate).
The only things left are to create a shared memory mapping and implement an allocator working in it.

This is safe as long as Lua knows nothing about parallelism. Some things in the standard library do know, please see the “Caveats” section.

On systems other that Linux, shared memory mappings have to be of fixed size. By default, this PoC allocates 16 Mb on these systems.

In fact, Linux shared memory mappings also have to be of fixed “size” — of fixed *virtual size*:
thanks to the [overcommit feature](https://www.kernel.org/doc/Documentation/vm/overcommit-accounting) and the [`MAP_NORESERVE`](http://man7.org/linux/man-pages/man2/mmap.2.html) `mmap` flag,
which tells Linux to only allocate physical pages on demand (this does *not* depend on which overcommit policy your system is configured to use), we can only pay for what we use,
and not a page more. And with [`madvise(..., MADV_REMOVE)`](http://man7.org/linux/man-pages/man2/madvise.2.html), we can reclaim the pages we don’t need anymore. Yay!

On Linux, this PoC allocates 16 Gb of *virtual* memory as this ought to be enough for anybody.
If your virtual memory is limited, then run it as `./main -p`. This forces it to fall back to a portable 16 Mb mapping.

Caveats
===
* Some functions in Lua’s standard library *really* don’t expect being called with such a setup. These functions are `os.execute`, `io.popen`, `os.exit`, and `os.setlocale`.
* All actions on files, including opening and closing, are local to the current process.
* My memory allocator is not optimal. Implementing a better one — possibly leveraging the aforementioned Linux features — is left as an exercise for the reader.
* Does not work with LuaJIT.
* Be careful with external modules.
