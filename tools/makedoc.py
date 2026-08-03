#!/usr/bin/env python3
"""Build the NYamy manual: Markdown sources -> a single HTML page.

Usage:
    py tools/makedoc.py <src-dir> <out-dir>

    <src-dir>   doc/src   - contains manual-ja/*.md, template.html, style.css
    <out-dir>   doc       - receives README-ja.html and style.css

Requires Python-Markdown (see tools/requirements.txt).  No other dependency.

Output is written as UTF-8 without BOM and with LF line endings so that it
matches the `*.html eol=lf` rule in .gitattributes; anything else makes git
report the generated files as modified on every checkout.
"""

import html
import os
import re
import sys
import unicodedata
from html.parser import HTMLParser

try:
	import markdown
	from markdown.extensions import Extension
	from markdown.inlinepatterns import InlineProcessor
	from markdown.postprocessors import Postprocessor
	from markdown.extensions.toc import slugify_unicode
	import xml.etree.ElementTree as etree
except ImportError:
	sys.stderr.write(
		"makedoc: Python-Markdown is not installed.\n"
		"         Run: py -m pip install -r tools/requirements.txt\n")
	sys.exit(2)


MANUAL_DIR = "manual-ja"
TEMPLATE = "template.html"
STYLESHEET = "style.css"
OUTPUT = "README-ja.html"
HOME_LABEL = "← マニュアル"			# back link shown on the extra pages

# Markdown files the manual links to that should be readable as HTML too.
# Paths are relative to the output directory; each is rendered into it, and a
# link to the .md is rewritten to point at the generated .html.  Missing
# entries are skipped, so this list may name optional files.
EXTRA_PAGES = [
	("HISTORY-ja.md", "HISTORY-ja.html"),
	# Named after the product, not "README.html": README-ja.html is the manual
	# itself, and -nyamy/-ymgw/-yamy line the three READMEs up by ancestry.
	("../README.md", "README-nyamy.html"),
	("../README-ymgw.md", "README-ymgw.html"),
]

# Metavariables inside a ```mayu block are written as <KEY> using these
# brackets, because a Markdown code span cannot carry emphasis.  SampleExt
# turns them back into <em> when generating HTML.
META_OPEN = "⟨"
META_CLOSE = "⟩"


# --------------------------------------------------------------------------
# Markdown extensions
# --------------------------------------------------------------------------

KBD_RE = r"\[\[(.+?)\]\]"


class KbdInline(InlineProcessor):
	"""[[Control]] -> <kbd>Control</kbd>."""

	def handleMatch(self, m, data):
		el = etree.Element("kbd")
		el.text = m.group(1)
		return el, m.start(0), m.end(0)


class KbdExt(Extension):
	def extendMarkdown(self, md):
		# Priority must beat 'link' (160) so that [[x]] is not mistaken for a
		# shortcut reference link.
		md.inlinePatterns.register(KbdInline(KBD_RE, md), "kbd", 175)


SAMPLE_BLOCK_RE = re.compile(
	r'<pre><code class="language-mayu">(.*?)</code></pre>', re.DOTALL)
CODE_RE = re.compile(r"(<code[^>]*>)(.*?)(</code>)", re.DOTALL)
META_RE = re.compile(re.escape(META_OPEN) + r"(.+?)" + re.escape(META_CLOSE))


class SamplePost(Postprocessor):
	"""Restore the two things a code span cannot express in Markdown.

	```mayu blocks get the frame the old <p class="sample"> had, and the
	metavariable brackets become <em> again inside every code span, which is
	how the original manual wrote <code><em>KEY</em></code>.
	"""

	def run(self, text):
		text = SAMPLE_BLOCK_RE.sub(
			lambda m: '<pre class="sample"><code>%s</code></pre>' % m.group(1),
			text)
		return CODE_RE.sub(
			lambda m: m.group(1) + META_RE.sub(r"<em>\1</em>", m.group(2))
			+ m.group(3), text)


class SampleExt(Extension):
	def extendMarkdown(self, md):
		md.postprocessors.register(SamplePost(md), "sample", 25)


# --------------------------------------------------------------------------
# Emphasis checking
# --------------------------------------------------------------------------
#
# Python-Markdown accepts **bold** wherever it finds it, but CommonMark - and
# therefore markdown-it, and therefore the VS Code preview - only opens or
# closes emphasis on a "flanking" delimiter run.  Japanese trips over this in
# two ways, because CJK brackets and marks count as punctuation:
#
#     ...を**「窓使いの憂鬱」の...**確認    opener followed by 「, preceded by a letter
#     ...、**※**で...                       closer preceded by ※, followed by a letter
#
# Both render as bold in the generated HTML yet stay literal asterisks in the
# editor.  Write <strong>...</strong> in those spots instead.

EMPH_RE = re.compile(r"(\*\*|\*)(?!\s)((?:[^*]|\*(?!\1))+?)(?<!\s)\1")


def _is_ws(c):
    return c is None or c in " \t\n\v\f\r" or unicodedata.category(c) == "Zs"


def _is_punct(c):
    if c is None:
        return False
    if c in "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~":
        return True
    return unicodedata.category(c)[0] in "PS"


def _left_flanking(prev, nxt):
    if _is_ws(nxt):
        return False
    return (not _is_punct(nxt)) or _is_ws(prev) or _is_punct(prev)


def _right_flanking(prev, nxt):
    if _is_ws(prev):
        return False
    return (not _is_punct(prev)) or _is_ws(nxt) or _is_punct(nxt)


def _mask_code(line):
    return re.sub(r"`[^`]*`", lambda m: " " * len(m.group(0)), line)


def span_ok(masked, m):
    """True if CommonMark would treat this EMPH_RE match as emphasis."""
    mark = m.group(1)
    o_prev = masked[m.start() - 1] if m.start() else None
    o_next = masked[m.start() + len(mark)]
    c_prev = masked[m.end() - len(mark) - 1]
    c_next = masked[m.end()] if m.end() < len(masked) else None
    return (_left_flanking(o_prev, o_next)
            and _right_flanking(c_prev, c_next))


def prose_lines(text):
    """Yield (lineno, line, code-masked line) outside fenced blocks."""
    fenced = False
    for lineno, line in enumerate(text.split("\n"), 1):
        if line.lstrip().startswith("```"):
            fenced = not fenced
            continue
        if not fenced:
            yield lineno, line, _mask_code(line)


def bad_emphasis(text):
    """Emphasis spans CommonMark would leave as literal asterisks."""
    out = []
    for lineno, _line, masked in prose_lines(text):
        for m in EMPH_RE.finditer(masked):
            if not span_ok(masked, m):
                out.append((lineno, m.group(0)))
    return out


# --------------------------------------------------------------------------
# Link checking
# --------------------------------------------------------------------------

class RefCollector(HTMLParser):
	"""Collect anchors defined and references used by the generated page."""

	def __init__(self):
		super().__init__(convert_charrefs=True)
		self.ids = []
		self.hrefs = []
		self.srcs = []

	def handle_starttag(self, tag, attrs):
		a = dict(attrs)
		if a.get("id"):
			self.ids.append(a["id"])
		if tag == "a" and a.get("name"):
			self.ids.append(a["name"])
		if tag == "a" and a.get("href"):
			self.hrefs.append(a["href"])
		if a.get("src"):
			self.srcs.append(a["src"])


def check_links(page, out_dir):
	"""Return a list of problem descriptions (empty means everything resolves)."""
	c = RefCollector()
	c.feed(page)
	problems = []

	seen = set()
	for i in c.ids:
		if i in seen:
			problems.append("duplicate anchor id: #%s" % i)
		seen.add(i)

	for href in c.hrefs:
		if href.startswith("#"):
			if href[1:] and href[1:] not in seen:
				problems.append("dangling anchor: %s" % href)
			continue
		if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", href) or href.startswith("//"):
			continue					# external
		path, _, frag = href.partition("#")
		if path and not os.path.exists(os.path.join(out_dir, path)):
			problems.append("missing file: %s" % href)

	for src in c.srcs:
		if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", src) or src.startswith("//"):
			continue
		if not os.path.exists(os.path.join(out_dir, src)):
			problems.append("missing resource: %s" % src)

	return problems


# --------------------------------------------------------------------------
# Build
# --------------------------------------------------------------------------

def read_text(path):
	with open(path, "r", encoding="utf-8-sig") as f:
		return f.read()


def write_text(path, text):
	"""Always UTF-8 without BOM and LF, whatever the host platform prefers."""
	with open(path, "w", encoding="utf-8", newline="\n") as f:
		f.write(text)


def collect_sources(src_dir):
	d = os.path.join(src_dir, MANUAL_DIR)
	if not os.path.isdir(d):
		sys.stderr.write("makedoc: no such directory: %s\n" % d)
		sys.exit(1)
	names = sorted(n for n in os.listdir(d) if n.endswith(".md"))
	if not names:
		sys.stderr.write("makedoc: no .md files under %s\n" % d)
		sys.exit(1)
	return [os.path.join(d, n) for n in names]


def render(text):
	"""Markdown -> (title, toc html, body html)."""
	md = markdown.Markdown(
		extensions=["extra", "toc", "sane_lists", KbdExt(), SampleExt()],
		extension_configs={
			"toc": {"toc_depth": "2-4", "slugify": slugify_unicode},
		})
	body = md.convert(text)

	title = "NYamy"
	m = re.search(r"^#\s+(.+?)\s*(?:\{#.*\})?\s*$", text, re.MULTILINE)
	if m:
		title = m.group(1)
	return title, md.toc, body


def fill(template, title, toc, body, home, home_label):
	page = template
	page = page.replace("{{title}}", html.escape(title))
	page = page.replace("{{home}}", home)
	page = page.replace("{{home_label}}", html.escape(home_label or title))
	page = page.replace("{{toc}}", toc)
	page = page.replace("{{content}}", body)
	return page


LINK_RE = re.compile(r'((?:href|src)=")([^"]+)(")')


def relocate_links(page, src_dir, out_dir, md_pages):
	"""Fix up relative links after a page moves to out_dir.

	A page rendered from outside out_dir (README.md at the repo root, say)
	carries links relative to its own directory, so they are recomputed
	against out_dir.  Links to a Markdown file that is itself being rendered
	become links to the generated HTML, which is why the sources can keep
	plain .md links and stay clickable in an editor.
	"""
	def sub(m):
		href = m.group(2)
		if href.startswith("#") or href.startswith("//"):
			return m.group(0)
		if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", href):
			return m.group(0)
		path, sep, frag = href.partition("#")
		if not path:
			return m.group(0)
		full = os.path.abspath(os.path.join(src_dir, path))
		target = md_pages.get(os.path.normcase(full))
		if target is None:
			target = os.path.relpath(full, out_dir)
		return m.group(1) + target.replace(os.sep, "/") + sep + frag + m.group(3)

	return LINK_RE.sub(sub, page)


def build(src_dir, out_dir):
	sources = collect_sources(src_dir)
	chunks = [read_text(p).rstrip() for p in sources]
	text = "\n\n".join(chunks) + "\n"

	os.makedirs(out_dir, exist_ok=True)
	template = read_text(os.path.join(src_dir, TEMPLATE))

	# Markdown files linked from the manual that get an HTML rendering of
	# their own.  Sources are given relative to out_dir; every page is written
	# into out_dir, and relocate_links() rebases their relative links.
	extras = [(os.path.join(out_dir, s), os.path.join(out_dir, o))
			  for s, o in EXTRA_PAGES if os.path.exists(os.path.join(out_dir, s))]
	md_pages = {os.path.normcase(os.path.abspath(s)): os.path.basename(o)
				for s, o in extras}

	emphasis = []
	pages = []			# (output path, source dir, markdown text, home, label)
	pages.append((os.path.join(out_dir, OUTPUT), out_dir, text, "#top", None))
	for s, o in extras:
		pages.append((o, os.path.dirname(s), read_text(s), OUTPUT, HOME_LABEL))

	for path, chunk in zip(sources, chunks):
		for lineno, span in bad_emphasis(chunk):
			emphasis.append("%s:%d: %s is not emphasis in CommonMark; "
							"use <strong>/<em>" % (path, lineno, span))
	for s, _o in extras:
		for lineno, span in bad_emphasis(read_text(s)):
			emphasis.append("%s:%d: %s is not emphasis in CommonMark; "
							"use <strong>/<em>" % (s, lineno, span))

	written = []
	for out_path, page_src_dir, page_text, home, label in pages:
		title, toc, body = render(page_text)
		# Only the body carries links written relative to the source; the
		# template's own links are already relative to out_dir.
		body = relocate_links(body, page_src_dir, out_dir, md_pages)
		page = fill(template, title, toc, body, home, label)
		write_text(out_path, page)
		written.append((out_path, page))

	write_text(os.path.join(out_dir, STYLESHEET),
			   read_text(os.path.join(src_dir, STYLESHEET)))

	problems = []
	for out_path, page in written:
		for p in check_links(page, out_dir):
			problems.append("%s: %s" % (os.path.basename(out_path), p))

	print("makedoc: %d source file(s) -> %s"
		  % (len(sources), os.path.join(out_dir, OUTPUT)))
	for out_path, _page in written[1:]:
		print("makedoc: %s" % out_path)

	rc = 0
	if emphasis:
		sys.stderr.write("makedoc: %d emphasis problem(s) "
						 "(renders here, but not in a CommonMark editor):\n"
						 % len(emphasis))
		for e in emphasis:
			sys.stderr.write("  %s\n" % e)
		rc = 1
	if problems:
		sys.stderr.write("makedoc: %d link problem(s):\n" % len(problems))
		for p in problems:
			sys.stderr.write("  %s\n" % p)
		rc = 1
	if rc == 0:
		print("makedoc: link and emphasis checks ok")
	return rc


def main(argv):
	if len(argv) != 3:
		sys.stderr.write(__doc__)
		return 2
	return build(argv[1], argv[2])


if __name__ == "__main__":
	sys.exit(main(sys.argv))
