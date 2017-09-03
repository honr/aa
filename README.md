# AA

Side project: A ridiculously tiny and stupid build system.  Not meant for
production usage.  Will be doubtlessly over-engineered where it doesn't matter
and heavily under-featured where it would count.

Start with:

```shell
./bootstrap clang++-4.0
# or, say: ./bootstrap g++
aa
aa greet.hello
aa aa
```

If you put the resulting `.bin/aa` executable in your PATH, you can edit an
`AA` file in any directory and run `aa` there.
