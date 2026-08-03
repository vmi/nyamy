#!/usr/bin/env python3
"""ONE-SHOT converter: doc/*-ja.html -> doc/src/manual-ja/*.md.

THROWAWAY.  Delete this file once the migration has been reviewed; from then
on the pipeline is Markdown -> HTML only (tools/makedoc.py).

The old manual encodes its heading tree as nested
<dl><dt class="hN">title</dt><dd class="dN">body</dd></dl> rather than as
<h1>..<h4>, so a general-purpose HTML->Markdown converter leaves most of the
document as raw <div>/<dl>.  This script targets that specific scheme.

Usage:
    py tools/_html2md.py doc doc/src/manual-ja
"""

import os
import re
import sys
from html.parser import HTMLParser

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import makedoc									# CommonMark emphasis rules

META_OPEN = "⟨"
META_CLOSE = "⟩"

# Anchors defined in BOTH manual and customize.  The customize (reference)
# side keeps the name; the manual (tutorial) side is renamed.
RENAME = {"key": "tutorial-key", "mod": "tutorial-mod",
		  "modifier": "tutorial-modifier"}

VOID = {"br", "img", "hr", "meta", "link", "input"}
# HTML4 optional end tags: opening one of these implicitly closes the others.
AUTO_CLOSE = {
	"li": {"li"},
	"dt": {"dt", "dd"},
	"dd": {"dt", "dd"},
	"p": {"p"},
	"tr": {"tr", "td", "th"},
	"td": {"td", "th"},
	"th": {"td", "th"},
}


class Node:
	def __init__(self, tag, attrs=None):
		self.tag = tag
		self.attrs = dict(attrs or {})
		self.kids = []

	def cls(self):
		return self.attrs.get("class", "")

	def find_anchor(self):
		"""First descendant <a name=...>, or None."""
		for k in self.kids:
			if isinstance(k, Node):
				if k.tag == "a" and k.attrs.get("name"):
					return k
				r = k.find_anchor()
				if r is not None:
					return r
		return None

	def text(self):
		out = []
		for k in self.kids:
			out.append(k if isinstance(k, str) else k.text())
		return "".join(out)


class Builder(HTMLParser):
	def __init__(self):
		super().__init__(convert_charrefs=True)
		self.root = Node("#root")
		self.stack = [self.root]

	def handle_starttag(self, tag, attrs):
		# Only the *currently open* element may be closed implicitly; looking
		# deeper into the stack would tear apart a nested <dl>.
		while len(self.stack) > 1 and self.stack[-1].tag in AUTO_CLOSE.get(tag, ()):
			self.stack.pop()
		n = Node(tag, attrs)
		self.stack[-1].kids.append(n)
		if tag not in VOID:
			self.stack.append(n)

	def handle_startendtag(self, tag, attrs):
		self.stack[-1].kids.append(Node(tag, attrs))

	def handle_endtag(self, tag):
		if not any(n.tag == tag for n in self.stack[1:]):
			return
		while self.stack[-1].tag != tag:
			self.stack.pop()
		self.stack.pop()

	def handle_data(self, data):
		self.stack[-1].kids.append(data)


# --------------------------------------------------------------------------
# inline rendering
# --------------------------------------------------------------------------

ESCAPE_RE = re.compile(r"([\\*_\[\]`])")

# Placeholder for <br>: survives squash(), which would otherwise collapse a
# Markdown hard break ("  \n") into a single space.
BR = "\x00"


def unbr(s):
	return s.replace(BR, "  \n")


def esc(s):
	s = s.replace(" ", " ")
	# Angle brackets go in as entities rather than backslash escapes: Python-
	# Markdown's escape set covers ">" but not "<", so "\<" would leak the
	# backslash into the HTML.  Entities are understood by every parser and
	# also stop a leading ">" from starting a blockquote.
	s = s.replace("<", "&lt;").replace(">", "&gt;")
	return ESCAPE_RE.sub(r"\\\1", s)


def squash(s):
	s = re.sub(r"[ \t\r\n]+", " ", s)
	return s.replace(" " + BR, BR).replace(BR + " ", BR)


class Ctx:
	"""Per-source-file link rewriting rules."""

	def __init__(self, own):
		self.own = own				# "manual" or "customize"

	def href(self, h):
		if h.startswith("#"):
			frag = h[1:]
			if self.own == "manual" and frag in RENAME:
				frag = RENAME[frag]
			return "#" + frag
		m = re.match(r"^\.?/?(MANUAL-ja|CUSTOMIZE-ja)\.html(?:#(.*))?$", h)
		if m:
			frag = m.group(2)
			if not frag:
				return "#CUSTOMIZE" if m.group(1) == "CUSTOMIZE-ja" else "#ABSTRACT"
			if m.group(1) == "MANUAL-ja" and frag in RENAME:
				frag = RENAME[frag]
			return "#" + frag
		if h.startswith("../"):
			return h
		return h

	def name(self, n):
		if self.own == "manual" and n in RENAME:
			return RENAME[n]
		return n


def inline(node, ctx, in_code=False):
	"""Render a node's children as inline Markdown."""
	out = []
	for k in node.kids:
		if isinstance(k, str):
			out.append(squash(k) if in_code else esc(squash(k)))
			continue
		t = k.tag
		if t == "code":
			out.append("`" + inline(k, ctx, in_code=True).strip() + "`")
		elif t == "em":
			body = inline(k, ctx, in_code).strip()
			out.append(META_OPEN + body + META_CLOSE if in_code else "*" + body + "*")
		elif t == "strong":
			out.append("**" + inline(k, ctx, in_code).strip() + "**")
		elif t == "kbd":
			out.append("[[" + k.text().strip() + "]]")
		elif t == "u":
			# Accelerator underline in menu labels: keep it, Markdown has none.
			out.append("<u>" + inline(k, ctx, in_code) + "</u>")
		elif t == "br":
			out.append(BR)
		elif t == "img":
			out.append("![%s](%s)"
					   % (esc(k.attrs.get("alt", "")), k.attrs.get("src", "")))
		elif t == "a":
			if k.attrs.get("href"):
				body = inline(k, ctx, in_code).strip()
				out.append("[%s](%s)" % (body, ctx.href(k.attrs["href"])))
			elif k.attrs.get("name"):
				body = inline(k, ctx, in_code)
				out.append('<a name="%s"></a>%s' % (ctx.name(k.attrs["name"]), body))
			else:
				out.append(inline(k, ctx, in_code))
		elif t == "span":
			c = k.cls()
			body = inline(k, ctx, in_code)
			out.append('<span class="%s">%s</span>' % (c, body) if c else body)
		else:
			out.append(inline(k, ctx, in_code))
	return "".join(out)


# --------------------------------------------------------------------------
# block rendering
# --------------------------------------------------------------------------

def heading_of(dt, ctx):
	"""(level, text, id) for a <dt class="hN">."""
	m = re.match(r"h([1-4])$", dt.cls())
	if not m:
		return None
	level = int(m.group(1)) + 1			# h1 -> "##"
	a = dt.find_anchor()
	anchor = ctx.name(a.attrs["name"]) if a is not None else None
	# Render the title without the wrapping <a name>/<a href>.
	tmp = Node("dt")
	tmp.kids = list(dt.kids)
	if a is not None:
		def strip(n):
			out = []
			for k in n.kids:
				if k is a:
					inner = Node("span")
					inner.kids = list(a.kids)
					out.append(inner)
				elif isinstance(k, Node):
					strip(k)
					out.append(k)
				else:
					out.append(k)
			n.kids = out
		strip(tmp)
	title = squash(inline(tmp, ctx)).replace(BR, " ").strip()
	return level, title, anchor


def sample(node):
	"""<p class="sample"> -> a ```mayu fenced block."""
	buf = []
	for k in node.kids:
		if isinstance(k, str):
			buf.append(k.replace(" ", " "))
		elif k.tag == "br":
			buf.append("\n")
		elif k.tag == "em":
			buf.append(META_OPEN + k.text().strip() + META_CLOSE)
		else:
			buf.append(k.text())
	lines = [ln.strip() for ln in "".join(buf).split("\n")]
	while lines and not lines[0]:
		lines.pop(0)
	while lines and not lines[-1]:
		lines.pop()
	return "```mayu\n" + "\n".join(lines) + "\n```"


def default_settings_table(tbl, ctx):
	rows = []
	for tr in tbl.kids:
		if isinstance(tr, Node) and tr.tag == "tr":
			cells = [c for c in tr.kids if isinstance(c, Node) and c.tag in ("th", "td")]
			if len(cells) == 2:
				rows.append((cells[0].text().strip(), inline(cells[1], ctx).strip()))
	out = ["| 名前 | 設定ファイル名 | シンボル |", "|---|---|---|"]
	for i in range(0, len(rows) - 2, 3):
		out.append("| %s | %s | %s |" % (rows[i][1], rows[i + 1][1], rows[i + 2][1]))
	return "\n".join(out)


def listing(node, ctx, depth=0):
	ordered = node.tag == "ol"
	pad = "    " * depth
	out = []
	n = 0
	for k in node.kids:
		if not isinstance(k, Node) or k.tag != "li":
			continue
		n += 1
		marker = ("%d. " % n) if ordered else "- "
		sub = []
		own = Node("li")
		for c in k.kids:
			if isinstance(c, Node) and c.tag in ("ul", "ol"):
				sub.append(listing(c, ctx, depth + 1))
			else:
				own.kids.append(c)
		body = unbr(squash(inline(own, ctx)).strip())
		cont = pad + " " * len(marker)
		body = body.replace("\n", "\n" + cont)
		out.append(pad + marker + body)
		out.extend(sub)
	return "\n".join(out)


def blocks(node, ctx, out):
	"""Render block-level children of `node`, appending markdown to `out`."""
	for k in node.kids:
		if isinstance(k, str):
			if k.strip():
				out.append(esc(squash(k)).strip())
			continue
		t = k.tag
		if t == "div":
			blocks(k, ctx, out)
		elif t == "dl":
			render_dl(k, ctx, out)
		elif t == "p":
			if "sample" in k.cls():
				out.append(sample(k))
			else:
				body = unbr(squash(inline(k, ctx)).strip())
				if body:
					out.append(body)
		elif t in ("ul", "ol"):
			out.append(listing(k, ctx))
		elif t == "table":
			out.append(default_settings_table(k, ctx))
		elif t == "blockquote":
			inner = []
			blocks(k, ctx, inner)
			out.append("\n".join("> " + ln for ln in "\n\n".join(inner).split("\n")))
		elif t == "hr":
			out.append("---")
		elif t == "pre":
			out.append("```\n" + k.text().strip("\n") + "\n```")
		else:
			blocks(k, ctx, out)


def render_dl(dl, ctx, out):
	"""A <dl> is either a heading tree (dt.hN/dd.dN) or a definition list."""
	pairs = []
	for k in dl.kids:
		if isinstance(k, Node) and k.tag in ("dt", "dd"):
			pairs.append(k)

	is_headings = any(re.match(r"h[1-4]$", p.cls()) for p in pairs if p.tag == "dt")

	i = 0
	while i < len(pairs):
		node = pairs[i]
		if node.tag != "dt":
			i += 1
			continue
		body = pairs[i + 1] if i + 1 < len(pairs) and pairs[i + 1].tag == "dd" else None
		i += 2 if body is not None else 1

		if is_headings:
			h = heading_of(node, ctx)
			if h is None:					# untitled dt inside a heading dl
				out.append(unbr(squash(inline(node, ctx)).strip()))
			else:
				level, title, anchor = h
				line = "#" * level + " " + title
				if anchor:
					line += " {#%s}" % anchor
				out.append(line)
			if body is not None:
				blocks(body, ctx, out)
		else:
			a = node.find_anchor()
			term = squash(inline(node, ctx)).replace(BR, " ").strip()
			if a is not None and not a.attrs.get("href") and "<a name=" not in term:
				term = '<a name="%s"></a>%s' % (ctx.name(a.attrs["name"]), term)
			out.append(term)
			if body is not None:
				inner = []
				blocks(body, ctx, inner)
				text = "\n\n".join(inner)
				first = True
				chunk = []
				for para in text.split("\n\n"):
					prefix = ":   " if first else "    "
					first = False
					lines = para.split("\n")
					chunk.append(prefix + lines[0])
					chunk.extend("    " + ln for ln in lines[1:])
				out.append("\n\n".join(chunk) if chunk else ":   ")


# --------------------------------------------------------------------------
# driver
# --------------------------------------------------------------------------

def parse(path):
	with open(path, "r", encoding="utf-8-sig") as f:
		b = Builder()
		b.feed(f.read())
		return b.root


def find_div(root, cls):
	def walk(n):
		if isinstance(n, Node):
			if n.tag == "div" and cls in n.cls().split():
				return n
			for k in n.kids:
				r = walk(k)
				if r is not None:
					return r
		return None
	return walk(root)


def main_div(root):
	return find_div(root, "main")


def fix_emphasis(text):
	"""Rewrite *..* / **..** that CommonMark would not recognise as <em>/<strong>.

	Japanese hits this whenever a delimiter touches a CJK bracket or mark:
	Python-Markdown still bolds it, but a CommonMark editor shows the literal
	asterisks.  makedoc.py rejects such spans, so emit HTML for them instead.
	"""
	lines = text.split("\n")
	fenced = False
	for i, line in enumerate(lines):
		if line.lstrip().startswith("```"):
			fenced = not fenced
			continue
		if fenced:
			continue
		masked = makedoc._mask_code(line)		# same length, so offsets align
		edits = [m for m in makedoc.EMPH_RE.finditer(masked)
				 if not makedoc.span_ok(masked, m)]
		for m in reversed(edits):				# right to left keeps offsets valid
			tag = "strong" if m.group(1) == "**" else "em"
			lines[i] = (lines[i][:m.start()]
						+ "<%s>%s</%s>" % (tag, m.group(2), tag)
						+ lines[i][m.end():])
	return "\n".join(lines)


def joined(out):
	text = "\n\n".join(x for x in out if x.strip())
	text = re.sub(r"\n{3,}", "\n\n", text)
	return fix_emphasis(text.strip()) + "\n"


def split_chapters(md):
	"""Split rendered markdown on '## ' boundaries."""
	parts = re.split(r"(?m)^(?=## )", md)
	front = parts[0].strip()
	chapters = [p.strip() for p in parts[1:] if p.strip()]
	return front, chapters


def write(path, text):
	os.makedirs(os.path.dirname(path), exist_ok=True)
	with open(path, "w", encoding="utf-8", newline="\r\n") as f:
		f.write(text)
	print("  wrote %s (%d bytes)" % (path, len(text)))


HISTORY_STUB = """## 15. history {#HISTORY}

「窓使いの憂鬱」3.30 までの変更履歴は [HISTORY-ja.md](HISTORY-ja.md) に収録しています。

NYamy での変更点は [README.md](../README.md) を参照してください。
"""

# The appendix only ever existed in CONTENTS-ja.html, the navigation pane of
# the frameset -- neither MANUAL-ja.html nor CUSTOMIZE-ja.html contains it.
# Now that the table of contents is generated from the headings, it has to be
# real content, so it is reproduced here.
APPENDIX = """## appendix {#APPENDIX}

### sample settings {#appendix_samples}

- [`dot.mayu`](../dot.mayu)
- [`default.mayu`](../default.mayu)
- [`emacsedit.mayu`](../emacsedit.mayu)
- [`104.mayu`](../104.mayu)
- [`109.mayu`](../109.mayu)
- [`109on104.mayu`](../109on104.mayu)
- [`104on109.mayu`](../104on109.mayu)

### syntax {#appendix_syntax}

- [`syntax.txt`](syntax.txt)

### emacs mode {#appendix_emacs}

- [`mayu-mode.el`](../mayu-mode.el)

### user contributions {#appendix_contrib}

- [`contrib/109onAX.mayu`](../contrib/109onAX.mayu)
- [`contrib/98x1.mayu`](../contrib/98x1.mayu)
- [`contrib/ax.mayu`](../contrib/ax.mayu)
- [`contrib/dvorak.mayu`](../contrib/dvorak.mayu)
- [`contrib/DVORAKon109.mayu`](../contrib/DVORAKon109.mayu)
- [`contrib/keitai.mayu`](../contrib/keitai.mayu)
- [`contrib/mayu-settings.txt`](../contrib/mayu-settings.txt)
"""

SLUGS = ["abstract", "install", "uninstall", "menu", "tutorial", "faq",
		 "customize", "security", "bugs", "related-work", "references",
		 "copyright", "support", "acknowledgements", "history"]


def main(argv):
	if len(argv) != 3:
		sys.stderr.write(__doc__)
		return 2
	doc_dir, out_dir = argv[1], argv[2]

	man_root = parse(os.path.join(doc_dir, "MANUAL-ja.html"))
	man = main_div(man_root)
	cus = main_div(parse(os.path.join(doc_dir, "CUSTOMIZE-ja.html")))

	ctx = Ctx("manual")
	head = ["# 窓使いの憂鬱 - マニュアル"]
	for cls in ("title", "copyright"):
		d = find_div(man_root, cls)
		if d is not None:
			head.append(unbr(squash(inline(d, ctx)).strip()))

	o = []
	blocks(man, ctx, o)
	front, man_chapters = split_chapters(joined(o))
	front = "\n\n".join(head + ([front] if front else []))

	o = []
	blocks(cus, Ctx("customize"), o)
	_, cus_chapters = split_chapters(joined(o))

	print("manual: %d chapters, customize: %d chapters"
		  % (len(man_chapters), len(cus_chapters)))

	write(os.path.join(out_dir, "00-front.md"), front + "\n")

	for idx, chap in enumerate(man_chapters, start=1):
		if idx == 7:					# stub -> replaced by CUSTOMIZE-ja.html
			if cus_chapters:
				write(os.path.join(out_dir, "07-customize.md"),
					  cus_chapters[0] + "\n")
			continue
		if idx == 15:
			# The change log is 37% of the manual and stops at 3.30 (2005).
			# Move it out of the single page and leave a pointer behind.
			hist = re.sub(r"\[([^\]]*)\]\(#[^)]*\)", r"\1", chap)
			hist = re.sub(r"(?m)^## ", "# ", hist, count=1)
			write(os.path.join(doc_dir, "HISTORY-ja.md"), hist + "\n")
			write(os.path.join(out_dir, "15-history.md"), HISTORY_STUB)
			continue
		name = "%02d-%s.md" % (idx, SLUGS[idx - 1])
		write(os.path.join(out_dir, name), chap + "\n")

	write(os.path.join(out_dir, "16-appendix.md"), APPENDIX)
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
