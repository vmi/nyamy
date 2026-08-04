#!/usr/bin/env python3
"""Renumber the manual chapters after sources are added, removed or reordered.

Usage:
    py tools/renumdoc.py <src-dir> [--apply]

    <src-dir>   doc/src   - contains manual-ja/*.md

Nothing but the two-digit filename prefix decides the chapter order: makedoc.py
concatenates sorted(os.listdir()), so a gap in the prefixes is harmless to the
build.  The chapter number is spelled out a second time in each file's
"## N. title" heading, though, and that one is visible to the reader.  This
keeps the two in step:

    the first file  - front matter, an unnumbered "# title", keeps prefix 00
    prefix 01..NN   - "## N. title", N matching the prefix
    the appendix    - takes the next prefix slot but stays unnumbered

A file whose heading carries no number is renamed and otherwise left alone,
which is what holds for the front matter and the appendix.  Relative order is
always preserved; only the numbers move.

Renaming uses `git mv` so the history follows the file, and the <None> list in
proj/makedoc.vcxproj is resynchronised because Visual Studio shows it.  Without
--apply nothing is written and the plan is printed instead.

Heading and project file are patched as raw bytes, one line at a time, so that
the CRLF endings and the UTF-8 BOM these files carry survive untouched.  No
third-party module is needed - unlike makedoc.py this runs on a bare Python.
"""

import os
import re
import subprocess
import sys


MANUAL_DIR = "manual-ja"
VCXPROJ = os.path.join("proj", "makedoc.vcxproj")

NAME_RE = re.compile(r"^(\d+)-(.+)\.md$")

# The chapter number in the first heading of a file, e.g. "## 7. Ruby DSL".
HEAD_RE = re.compile(rb"^(##[ \t]+)(\d+)(\.[ \t]+)")

# A manual source listed for display in the project file.  The list has no
# effect on the build - the build reads the wildcard <DocSource> above it.
NONE_MD_RE = re.compile(
	rb'^([ \t]*)<None Include="\.\.\\doc\\src\\manual-ja\\[^"]+\.md"\s*/>\s*$')
NONE_MAKEDOC_RE = re.compile(
	rb'^[ \t]*<None Include="\.\.\\tools\\makedoc\.py"\s*/>\s*$')


class Entry(object):
	def __init__(self, old_name, num, stem):
		self.old_name = old_name
		self.num = num					# prefix as written today
		self.stem = stem				# name without prefix and extension
		self.new_name = old_name
		self.old_chapter = None			# None when the heading has no number
		self.new_chapter = None


def read_first_line(path):
	with open(path, "rb") as f:
		return f.readline()


def collect(d):
	"""Manual sources, ordered by their current prefix."""
	if not os.path.isdir(d):
		sys.stderr.write("renumdoc: no such directory: %s\n" % d)
		return None
	entries = []
	bad = []
	for name in sorted(os.listdir(d)):
		if not name.endswith(".md"):
			continue
		m = NAME_RE.match(name)
		if not m:
			bad.append(name)
			continue
		entries.append(Entry(name, int(m.group(1)), m.group(2)))
	if bad:
		# Renumbering these would mean guessing where they belong, and a file
		# without a prefix sorts ahead of every chapter, so stop instead.
		sys.stderr.write("renumdoc: %s: file name is not NN-name.md:\n" % d)
		for name in bad:
			sys.stderr.write("  %s\n" % name)
		return None
	if not entries:
		sys.stderr.write("renumdoc: no .md files under %s\n" % d)
		return None
	entries.sort(key=lambda e: (e.num, e.old_name))
	return entries


def plan(d, entries):
	"""Assign new prefixes and chapter numbers.  Returns a list of warnings."""
	width = max(2, len("%d" % (len(entries) - 1)))
	chapter = 0
	for i, e in enumerate(entries):
		line = read_first_line(os.path.join(d, e.old_name))
		m = HEAD_RE.match(line)
		if m:
			e.old_chapter = int(m.group(2))
			chapter += 1
			e.new_chapter = chapter
		e.new_name = "%0*d-%s.md" % (width, i, e.stem)

	warnings = []
	for i, e in enumerate(entries):
		if e.new_chapter is not None and e.new_chapter != i:
			# Holds as long as exactly one unnumbered file (the front matter)
			# comes before the chapters.  Say so rather than quietly emitting a
			# heading that disagrees with its own file name.
			warnings.append(
				"%s: chapter %d does not match prefix %0*d"
				% (e.new_name, e.new_chapter, width, i))
	return warnings


def show(d, entries):
	changed = 0
	for e in entries:
		renamed = e.new_name != e.old_name
		renumbered = (e.old_chapter is not None
					  and e.new_chapter != e.old_chapter)
		if not renamed and not renumbered:
			continue
		changed += 1
		if renamed:
			what = "%-24s -> %-24s" % (e.old_name, e.new_name)
		else:
			what = "%-24s    %-24s" % (e.old_name, "")
		if renumbered:
			what += "## %d. -> ## %d." % (e.old_chapter, e.new_chapter)
		print("  " + what.rstrip())
	if not changed:
		print("renumdoc: %s: already sequential" % d)
	return changed


def rewrite_heading(path, new_chapter):
	"""Replace the number in the first heading, leaving every other byte be."""
	with open(path, "rb") as f:
		data = f.read()
	head, sep, rest = data.partition(b"\n")
	m = HEAD_RE.match(head)
	if not m:
		return False
	head = (m.group(1) + str(new_chapter).encode("ascii") + m.group(3)
			+ head[m.end():])
	with open(path, "wb") as f:
		f.write(head + sep + rest)
	return True


def move(d, old, new):
	"""git mv, so the rename stays visible in the history."""
	try:
		r = subprocess.run(["git", "mv", "--", old, new], cwd=d,
						   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		if r.returncode == 0:
			return
	except OSError:
		pass			# no git on PATH
	os.rename(os.path.join(d, old), os.path.join(d, new))


def rename_all(d, entries):
	"""Rename via temporary names so that a swap cannot clobber a file."""
	moving = [e for e in entries if e.new_name != e.old_name]
	tmp = {}
	for e in moving:
		t = e.new_name + ".renumdoc-tmp"
		move(d, e.old_name, t)
		tmp[e.new_name] = t
	for e in moving:
		move(d, tmp[e.new_name], e.new_name)
	return len(moving)


def update_vcxproj(path, entries):
	"""Resynchronise the <None> list Visual Studio shows in Solution Explorer."""
	if not os.path.isfile(path):
		sys.stderr.write("renumdoc: %s not found, skipping\n" % path)
		return False
	with open(path, "rb") as f:
		lines = f.readlines()

	hits = [i for i, ln in enumerate(lines) if NONE_MD_RE.match(ln)]
	if not hits:
		sys.stderr.write("renumdoc: %s: no manual entries in the <None> list, "
						 "skipping\n" % path)
		return False

	first, last = hits[0], hits[-1]
	indent = NONE_MD_RE.match(lines[first]).group(1)
	eol = b"\r\n" if lines[first].endswith(b"\r\n") else b"\n"
	block = [indent + b'<None Include="..\\doc\\src\\manual-ja\\'
			 + e.new_name.encode("ascii") + b'" />' + eol for e in entries]
	lines[first:last + 1] = block

	# List this tool next to the one it maintains.
	if not any(b"renumdoc.py" in ln for ln in lines):
		for i, ln in enumerate(lines):
			if NONE_MAKEDOC_RE.match(ln):
				lines.insert(i + 1, indent
							 + b'<None Include="..\\tools\\renumdoc.py" />'
							 + eol)
				break

	with open(path, "wb") as f:
		f.writelines(lines)
	return True


def main(argv):
	args = [a for a in argv[1:] if not a.startswith("--")]
	flags = [a for a in argv[1:] if a.startswith("--")]
	apply_ = "--apply" in flags
	if len(args) != 1 or set(flags) - {"--apply"}:
		sys.stderr.write(__doc__)
		return 2

	src_dir = args[0]
	d = os.path.join(src_dir, MANUAL_DIR)
	entries = collect(d)
	if entries is None:
		return 2

	warnings = plan(d, entries)
	print("renumdoc: %s: %d file(s)" % (d, len(entries)))
	changed = show(d, entries)
	for w in warnings:
		sys.stderr.write("renumdoc: warning: %s\n" % w)

	if not changed:
		return 0
	if not apply_:
		print("renumdoc: dry run; re-run with --apply to write these changes")
		return 0

	for e in entries:
		if e.old_chapter is not None and e.new_chapter != e.old_chapter:
			rewrite_heading(os.path.join(d, e.old_name), e.new_chapter)
	moved = rename_all(d, entries)

	# proj/ is a sibling of doc/, so the repo root is two levels above doc/src.
	root = os.path.dirname(os.path.dirname(os.path.abspath(src_dir)))
	update_vcxproj(os.path.join(root, VCXPROJ), entries)

	print("renumdoc: %d file(s) renamed; regenerate with "
		  "tools\\ps1exec.cmd tools\\makedoc.ps1 doc\\src doc" % moved)
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
