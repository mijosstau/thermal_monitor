#!/usr/bin/env python3
"""Render docs/README.md to a simple PDF without external PDF tools."""

from __future__ import annotations

import argparse
import re
import textwrap
from pathlib import Path


PAGE_W = 595
PAGE_H = 842
MARGIN = 54
CONTENT_W = PAGE_W - (2 * MARGIN)


def pdf_hex(text: str) -> str:
    data = text.encode("cp1252", errors="replace")
    return "<" + data.hex().upper() + ">"


def text_width(text: str, font_size: int) -> float:
    return len(text) * font_size * 0.52


def wrap_text(text: str, font_size: int, indent: int = 0) -> list[str]:
    usable = max(20, CONTENT_W - indent)
    chars = max(20, int(usable / (font_size * 0.52)))
    return textwrap.wrap(text, width=chars, replace_whitespace=False) or [""]


def strip_inline_markdown(text: str) -> str:
    text = re.sub(r"`([^`]+)`", r"\1", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"\1", text)
    text = re.sub(r"\*([^*]+)\*", r"\1", text)
    text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", text)
    return text


class PdfWriter:
    def __init__(self) -> None:
        self.pages: list[list[str]] = []
        self.current: list[str] = []
        self.y = PAGE_H - MARGIN

    def new_page(self) -> None:
        if self.current:
            self.pages.append(self.current)
        self.current = []
        self.y = PAGE_H - MARGIN

    def ensure_space(self, height: int) -> None:
        if self.y - height < MARGIN:
            self.new_page()

    def add_line(self, text: str, font: str = "F1", size: int = 10, indent: int = 0) -> None:
        line_height = int(size * 1.35)
        self.ensure_space(line_height)
        x = MARGIN + indent
        self.current.append(f"BT /{font} {size} Tf {x} {self.y} Td {pdf_hex(text)} Tj ET")
        self.y -= line_height

    def add_wrapped(self, text: str, font: str = "F1", size: int = 10, indent: int = 0) -> None:
        for line in wrap_text(text, size, indent):
            self.add_line(line, font, size, indent)

    def add_space(self, height: int) -> None:
        self.ensure_space(height)
        self.y -= height

    def add_rule(self) -> None:
        self.ensure_space(12)
        self.current.append(f"{MARGIN} {self.y} m {PAGE_W - MARGIN} {self.y} l S")
        self.y -= 12

    def finish(self) -> bytes:
        if self.current or not self.pages:
            self.pages.append(self.current)

        objects: list[bytes] = []

        def add(obj: str | bytes) -> int:
            if isinstance(obj, str):
                obj = obj.encode("latin-1")
            objects.append(obj)
            return len(objects)

        catalog_id = add("<< /Type /Catalog /Pages 2 0 R >>")
        pages_id = 2
        add(b"")
        font_helvetica_id = add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
        font_bold_id = add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>")
        font_courier_id = add("<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>")

        page_ids: list[int] = []
        for page in self.pages:
            content = "\n".join(page).encode("latin-1")
            content_id = add(
                b"<< /Length " + str(len(content)).encode("ascii") + b" >>\nstream\n"
                + content
                + b"\nendstream"
            )
            page_id = add(
                f"<< /Type /Page /Parent {pages_id} 0 R "
                f"/MediaBox [0 0 {PAGE_W} {PAGE_H}] "
                f"/Resources << /Font << /F1 {font_helvetica_id} 0 R "
                f"/F2 {font_bold_id} 0 R /F3 {font_courier_id} 0 R >> >> "
                f"/Contents {content_id} 0 R >>"
            )
            page_ids.append(page_id)

        kids = " ".join(f"{page_id} 0 R" for page_id in page_ids)
        objects[pages_id - 1] = f"<< /Type /Pages /Kids [{kids}] /Count {len(page_ids)} >>".encode("latin-1")

        out = bytearray(b"%PDF-1.4\n%\xE2\xE3\xCF\xD3\n")
        offsets = [0]
        for idx, obj in enumerate(objects, start=1):
            offsets.append(len(out))
            out.extend(f"{idx} 0 obj\n".encode("ascii"))
            out.extend(obj)
            out.extend(b"\nendobj\n")

        xref = len(out)
        out.extend(f"xref\n0 {len(objects) + 1}\n".encode("ascii"))
        out.extend(b"0000000000 65535 f \n")
        for offset in offsets[1:]:
            out.extend(f"{offset:010d} 00000 n \n".encode("ascii"))
        out.extend(
            f"trailer\n<< /Size {len(objects) + 1} /Root {catalog_id} 0 R >>\n"
            f"startxref\n{xref}\n%%EOF\n".encode("ascii")
        )
        return bytes(out)


def render_markdown(markdown: str) -> bytes:
    pdf = PdfWriter()
    in_code = False

    for raw_line in markdown.splitlines():
        line = raw_line.rstrip()

        if line.startswith("```"):
            in_code = not in_code
            pdf.add_space(6)
            continue

        if in_code:
            pdf.add_wrapped(line, font="F3", size=8)
            continue

        if not line.strip():
            pdf.add_space(7)
            continue

        if line.startswith("# "):
            pdf.add_wrapped(strip_inline_markdown(line[2:]), font="F2", size=20)
            pdf.add_rule()
            continue
        if line.startswith("## "):
            pdf.add_space(8)
            pdf.add_wrapped(strip_inline_markdown(line[3:]), font="F2", size=15)
            continue
        if line.startswith("### "):
            pdf.add_space(5)
            pdf.add_wrapped(strip_inline_markdown(line[4:]), font="F2", size=12)
            continue

        if line.startswith("- "):
            pdf.add_wrapped("- " + strip_inline_markdown(line[2:]), size=10, indent=12)
            continue

        if re.match(r"^\d+\. ", line):
            pdf.add_wrapped(strip_inline_markdown(line), size=10, indent=12)
            continue

        if line.startswith("|"):
            pdf.add_wrapped(line, font="F3", size=8)
            continue

        pdf.add_wrapped(strip_inline_markdown(line), size=10)

    return pdf.finish()


def main() -> int:
    parser = argparse.ArgumentParser(description="Render docs/README.md to docs/README.pdf.")
    parser.add_argument("--input", default="docs/README.md", help="Markdown input file")
    parser.add_argument("--output", default="docs/README.pdf", help="PDF output file")
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)
    markdown = input_path.read_text(encoding="utf-8")
    output_path.write_bytes(render_markdown(markdown))
    print(f"Wrote {output_path} from {input_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
