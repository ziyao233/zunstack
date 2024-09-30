# zunstack

Unwind the stack on fatal signals!

***Currently in demo stage, don't use it in production.***

## Try it!

An example is included in `test.c`, compile it together with zunstack, e.g.

```
$ clang test.c zunstack.c -o test \
	-fno-omit-frame-pointer		\
	-Wl,--export-dynamic		\
	-Wl,--hash-style=both
$ ./test
Frame #0 (0x0x7fffd3a453b0): PC = 0x5596779e3adc
	symbol foo in ./test
Frame #1 (0x0x7fffd3a453e0): PC = 0x5596779e3b0d
        symbol main in ./test
Aborted
```

## How does it work

### Stack Unwind

Most compilers will generate code in a fixed pattern if
`-fno-omit-frame-pointer` is specified:

- Stackframe is always hold by a register, called FP (frame pointer).
  On x86_64 it is %rbp.
- Address of caller's frame and PC could be indexed by FP

If all DSO are compiled with `-fno-omit-frame-pointer`, we could simply
walk through the stack like iterating a linked list.

### Symbol Resolve

zunstack does not depend on debuginfo to resolve symbol information.
Instead, it iterates all loaded DSO and check whether a dynamic symbols'
address region covers PC.

## Limitations

- `-fno-omit-frame-pointer` must be applied for all DSOs. CFI-based
  unwinding could be added later to avoid this.
- Only exported dynamic symbols could be resolved.
  (force symbols in exexcutable to be exported by `-Wl,--export-dynamic`)
- Currently, symbol resolving supports only traditional ELF hashtable.
  (`-Wl,--hash-style=both` ensures traditional ELF hashtable is generated)

## Compatiblity

Tested on x86_64 [eweOS](https://os.ewe.moe) (musl-based Linux distro).
Only musl and x86_64 is supported for now.

## Copyright

```
Copyright (c) 2024 Yao Zi. All rights reserved.
zunstack is distributed under Mozilla Public License Version 2.0.
```
