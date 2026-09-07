#!/usr/bin/env python3
"""Generate and validate compile_commands.json without shell utilities."""

import argparse
import json
import shlex
from pathlib import Path


SUPPORTED_ARCHES = ("riscv", "loongarch")


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--arch", choices=SUPPORTED_ARCHES, required=True)
    parser.add_argument("--generated-include", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--flags", default="")
    parser.add_argument("--sources", nargs="+", required=True)
    return parser.parse_args()


def validate_flags(flags, arch, generated_include):
    required = {f"-Iarch/{arch}/include", f"-I{generated_include}"}
    missing = required.difference(flags)
    if missing:
        raise ValueError(
            "compile commands are missing required include paths: "
            + ", ".join(sorted(missing))
        )

    leaked = {
        f"-Iarch/{other}/include"
        for other in SUPPORTED_ARCHES
        if other != arch and f"-Iarch/{other}/include" in flags
    }
    if leaked:
        raise ValueError(
            "compile commands contain include paths from another architecture: "
            + ", ".join(sorted(leaked))
        )


def main():
    args = parse_args()
    flags = shlex.split(args.flags, posix=True)
    validate_flags(flags, args.arch, args.generated_include)

    directory = Path(args.directory).resolve().as_posix()
    entries = [
        {
            "directory": directory,
            "arguments": [args.compiler, *flags, "-c", source],
            "file": source,
        }
        for source in args.sources
    ]

    args.output.write_text(
        json.dumps(entries, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    # Read the file back so serialization or write failures are caught here.
    with args.output.open(encoding="utf-8") as generated:
        json.load(generated)

    print(f"Generated {args.output} with {len(entries)} entries for ARCH={args.arch}")


if __name__ == "__main__":
    main()
