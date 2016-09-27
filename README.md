# Unison Driver for LLVM

This branch contains a Unison driver built on top of LLVM's `llc` code
generator. The core Unison tool can be found in the [Unison
repository](https://github.com/unison-code/unison).

## Building, Testing, and Installing

Just checkout one of the branches with a `-unison` suffix (corresponding to
upstream LLVM's master and release branches) and follow the instructions
provided at LLVM's [website](http://llvm.org/docs/GettingStarted.html) as usual.

## Running

Just run `llc` on the LLVM IR module to be compiled to assembly code with the
flag `-unison`. For example, to compile the LLVM IR module `foo.ll` to Hexagon
V4 assembly code with Unison, just run:

```
llc foo.ll -march=hexagon -mcpu=hexagonv4 -unison -o foo.s
```

Currently, Unison supports the following targets (defined by
`march`-`mcpu`-`mattr` triples):

| Target | `-march=` | `-mcpu=` | `-mattr=` |
| --- | --- | --- | ---  |
| Hexagon V4 | `hexagon` | `hexagonv4` | |
| ARM1156T2F-S | `arm` | `arm1156t2f-s` | `+thumb-mode` |
| MIPS32 | `mips` | `mips32` | |

The execution of Unison can also be controlled at the source level, where
functions can be annotated with the `"unison"` attribute to indicate that Unison
should be run on them:

```
__attribute__((annotate("unison")))
```

Other flags (with a `-unison` prefix) can be used to control the execution of
Unison, run `llc --help` for details.

## Contact

[Roberto Castañeda Lozano](https://www.sics.se/~rcas/) [<rcas@sics.se>]

## License

The Unison Driver is licensed under the BSD3 license, see the
[LICENSE.md](LICENSE.md) file for details.

## Further Reading

Check [the Unison website](https://unison-code.github.io/).