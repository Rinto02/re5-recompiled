#!/usr/bin/env python3
"""Validate the captured RE5 Xenos microcode archive before shader translation.

This checks archive integrity, stage pairing, dword alignment and disassembly
presence. It does not translate Xenos instructions to SPIR-V.
"""
from __future__ import annotations

import argparse
import json
import re
import zipfile
from collections import Counter
from pathlib import PurePosixPath

NAME = re.compile(r"^shader_([0-9A-Fa-f]{16})\.ucode(?:\.bin)?\.(vert|frag)$")


def audit(path: str) -> dict:
    with zipfile.ZipFile(path) as archive:
        bad = archive.testzip()
        if bad:
            raise ValueError(f"corrupt ZIP member: {bad}")
        groups: dict[tuple[str, str], dict[str, object]] = {}
        ignored = []
        for name in archive.namelist():
            if name.endswith("/"):
                continue
            match = NAME.match(PurePosixPath(name).name)
            if not match:
                ignored.append(name)
                continue
            digest, stage = match.groups()
            is_binary = ".bin." in name
            entry = groups.setdefault((digest.upper(), stage), {})
            key = "binary" if is_binary else "disassembly"
            if key in entry:
                raise ValueError(f"duplicate {key} for {digest}.{stage}")
            data = archive.read(name)
            entry[key] = {
                "path": name,
                "bytes": len(data),
            }
            if is_binary:
                if not data:
                    raise ValueError(f"empty microcode: {name}")
                if len(data) % 4:
                    raise ValueError(f"microcode is not dword-aligned: {name}")
                entry["dwords"] = len(data) // 4
            else:
                try:
                    text = data.decode("utf-8")
                except UnicodeDecodeError as exc:
                    raise ValueError(f"disassembly is not UTF-8: {name}") from exc
                if not text.strip():
                    raise ValueError(f"empty disassembly: {name}")
                entry["disassembly_lines"] = len(text.splitlines())

        missing_binary = []
        missing_disassembly = []
        shaders = []
        for (digest, stage), entry in sorted(groups.items()):
            if "binary" not in entry:
                missing_binary.append(f"{digest}.{stage}")
            if "disassembly" not in entry:
                missing_disassembly.append(f"{digest}.{stage}")
            shaders.append({"hash": digest, "stage": stage, **entry})

        if missing_binary or missing_disassembly:
            raise ValueError(
                f"incomplete shader pairs: missing binary={missing_binary}; "
                f"missing disassembly={missing_disassembly}"
            )

        return {
            "archive": path,
            "shader_count": len(shaders),
            "stage_counts": dict(Counter(s["stage"] for s in shaders)),
            "binary_bytes": sum(s["binary"]["bytes"] for s in shaders),
            "disassembly_bytes": sum(s["disassembly"]["bytes"] for s in shaders),
            "shaders": shaders,
        }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", help="Path to re5_shader_ucode.zip")
    parser.add_argument("--json", dest="json_path", help="Write complete JSON manifest")
    args = parser.parse_args()
    result = audit(args.archive)
    summary = {k: v for k, v in result.items() if k != "shaders"}
    print(json.dumps(summary, indent=2))
    if args.json_path:
        with open(args.json_path, "w", encoding="utf-8") as output:
            json.dump(result, output, indent=2)
            output.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
