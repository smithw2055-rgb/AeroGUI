#!/usr/bin/env python3
"""Count unique public Aero header lines in a start header's include-closure.

Skips includes that live only inside `#if defined(AERO_GUI_IMPLEMENTATION)`
(and `#ifdef AERO_GUI_IMPLEMENTATION`) so installed-header budgets match the
SDK consumer compile, not the library TU.
"""
from __future__ import annotations

import os
import re
import sys

INCLUDE_RE = re.compile(
    r'^[ \t]*#[ \t]*include[ \t]+<((?:Aero|AeroApp|AeroRender|AeroAudio)/[^>]+)>'
)
IFDEF_IMPL_RE = re.compile(
    r'^[ \t]*#[ \t]*(?:if[ \t]+defined\s*\(\s*AERO_GUI_IMPLEMENTATION\s*\)'
    r'|ifdef[ \t]+AERO_GUI_IMPLEMENTATION)\b'
)
IF_RE = re.compile(r'^[ \t]*#[ \t]*if(?:n?def)?\b')
ENDIF_RE = re.compile(r'^[ \t]*#[ \t]*endif\b')
ELSE_RE = re.compile(r'^[ \t]*#[ \t]*(?:else|elif)\b')
LINE_COMMENT_RE = re.compile(r'//.*')
BLOCK_COMMENT_RE = re.compile(r'/\*.*?\*/', re.DOTALL)


def strip_comments(text: str) -> str:
    text = BLOCK_COMMENT_RE.sub(' ', text)
    lines = []
    for line in text.splitlines():
        lines.append(LINE_COMMENT_RE.sub('', line))
    return '\n'.join(lines)


def public_includes(text: str) -> list[str]:
    skip_depth = 0
    if_depth = 0
    skip_at: list[int] = []
    found: list[str] = []
    for raw in strip_comments(text).splitlines():
        if IFDEF_IMPL_RE.match(raw):
            if_depth += 1
            skip_at.append(if_depth)
            skip_depth += 1
            continue
        if IF_RE.match(raw):
            if_depth += 1
            continue
        if ELSE_RE.match(raw):
            if skip_at and skip_at[-1] == if_depth:
                # Public counterpart of an IMPLEMENTATION-only branch.
                skip_depth = max(0, skip_depth - 1)
                skip_at.pop()
            continue
        if ENDIF_RE.match(raw):
            if skip_at and skip_at[-1] == if_depth:
                skip_depth = max(0, skip_depth - 1)
                skip_at.pop()
            if_depth = max(0, if_depth - 1)
            continue
        if skip_depth > 0:
            continue
        match = INCLUDE_RE.match(raw)
        if match:
            found.append('include/' + match.group(1))
    return found


def count_lines(path: str) -> int:
    with open(path, 'rb') as handle:
        return sum(1 for _ in handle)


def measure(source_root: str, start_relative: str) -> tuple[int, int]:
    queue = [start_relative]
    seen: set[str] = set()
    order: list[str] = []
    while queue:
        current = queue.pop(0)
        if current in seen:
            continue
        seen.add(current)
        full = os.path.join(source_root, current)
        if not os.path.isfile(full):
            continue
        order.append(current)
        with open(full, encoding='utf-8', errors='replace') as handle:
            text = handle.read()
        for nxt in public_includes(text):
            if nxt not in seen:
                queue.append(nxt)
    lines = 0
    for relative in order:
        lines += count_lines(os.path.join(source_root, relative))
    return lines, len(order)


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        sys.stderr.write(
            'usage: CountPublicIncludeClosure.py SOURCE_ROOT HEADER [HEADER...]\n')
        return 2
    source_root = argv[1]
    for header in argv[2:]:
        lines, files = measure(source_root, header)
        sys.stdout.write(f'{header} {lines} {files}\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
