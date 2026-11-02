# -*- coding: utf-8 -*-
"""把 开环FOC入门详解.md 转成排版好的 DOCX。"""
import re
import os
from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement
from PIL import Image, ImageDraw, ImageFont

SRC = r"D:\PMSMfoc\docs\开环FOC入门详解.md"
OUT = r"D:\PMSMfoc\docs\开环FOC入门详解.docx"
IMG_DIR = r"D:\PMSMfoc\docs\img"

# 允许命令行指定源文件和输出文件：
#   python build_docx.py <源.md> <输出.docx>
import sys
if len(sys.argv) >= 3:
    SRC = sys.argv[1]
    OUT = sys.argv[2]

# 含这些字符的代码块当作"框图"，渲染成图片保证对齐
BOXCHARS = set("─│┌┐└┘├┤┬┴┼▏▎▍▌▋▊▉█→↗↘←↖↙")

FONT_CANDIDATES = [
    r"C:\Windows\Fonts\msyh.ttc",
    r"C:\Windows\Fonts\simhei.ttf",
    r"C:\Windows\Fonts\simsun.ttc",
]


def _font(size):
    for p in FONT_CANDIDATES:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                pass
    return ImageFont.load_default()


def is_wide(ch):
    o = ord(ch)
    return (0x1100 <= o <= 0x115F) or (0x2E80 <= o <= 0xA4CF) or \
           (0xAC00 <= o <= 0xD7A3) or (0xF900 <= o <= 0xFAFF) or \
           (0xFE30 <= o <= 0xFE6F) or (0xFF00 <= o <= 0xFF60) or \
           (0xFFE0 <= o <= 0xFFE6) or (0x3000 <= o <= 0x303F)


def render_diagram(block, out_png, size=20):
    """逐字符按固定宽度绘制：西文/框线 1 格，中文 2 格，保证竖线对齐。"""
    font = _font(size)
    adv_latin = size * 0.55
    adv_wide = size * 1.10

    # 微软雅黑里缺这几个字形，换成等效字符
    FIX = {"\u2194": "~", "\u25BA": ">", "\u25C4": "<", "\u2713": "\u221A"}
    block = ["".join(FIX.get(c, c) for c in line) for line in block]

    maxw = 0.0
    for line in block:
        w = sum(adv_wide if is_wide(c) else adv_latin for c in line)
        maxw = max(maxw, w)

    line_h = size * 1.5
    W = int(maxw) + 28
    H = int(len(block) * line_h) + 24
    img = Image.new("RGB", (max(W, 60), max(H, 40)), (250, 250, 250))
    d = ImageDraw.Draw(img)
    y = 12
    for line in block:
        x = 14
        for c in line:
            d.text((x, y), c, font=font, fill=(25, 25, 25))
            x += adv_wide if is_wide(c) else adv_latin
        y += line_h
    img.save(out_png)
    return img.size


def add_diagram(doc, block):
    os.makedirs(IMG_DIR, exist_ok=True)
    name = "diag_%03d.png" % add_diagram.n
    add_diagram.n += 1
    path = os.path.join(IMG_DIR, name)
    pw, _ph = render_diagram(block, path)
    width_cm = pw * 2.54 / 96.0
    if width_cm > 16.2:
        width_cm = 16.2
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(6)
    p.paragraph_format.space_after = Pt(8)
    p.add_run().add_picture(path, width=Cm(width_cm))


add_diagram.n = 1

BODY_FONT = "宋体"
BODY_EA = "宋体"
HEAD_FONT = "微软雅黑"
HEAD_EA = "微软雅黑"
MONO = "新宋体"

# 把 emoji 换成宋体能显示的符号
REPL = {
    "⭐⭐⭐": "★★★", "⭐⭐": "★★", "⭐": "★",
    "✅": "√", "❌": "×", "✓": "√", "✗": "×",
    "⚠️": "!", "⚠": "!", "🎯": "◎", "📌": "★",
}


def sanitize(s: str) -> str:
    for k, v in REPL.items():
        s = s.replace(k, v)
    return s


def set_run_font(run, name, ea, size=None, bold=None, color=None):
    run.font.name = name
    rPr = run._element.get_or_add_rPr()
    rFonts = rPr.find(qn('w:rFonts'))
    if rFonts is None:
        rFonts = OxmlElement('w:rFonts')
        rPr.append(rFonts)
    rFonts.set(qn('w:ascii'), name)
    rFonts.set(qn('w:hAnsi'), name)
    rFonts.set(qn('w:eastAsia'), ea)
    if size is not None:
        run.font.size = Pt(size)
    if bold is not None:
        run.font.bold = bold
    if color is not None:
        run.font.color.rgb = color


def shade_paragraph(p, fill="F4F4F4"):
    pPr = p._p.get_or_add_pPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:color'), 'auto')
    shd.set(qn('w:fill'), fill)
    pPr.append(shd)


def add_inline(p, text, size=10.5):
    """处理 **粗体**、*斜体* 和 `行内代码`"""
    text = sanitize(text)
    parts = re.split(r'(\*\*.+?\*\*|`[^`]+`|\*[^*\s][^*]*\*)', text)
    for part in parts:
        if not part:
            continue
        if part.startswith('**') and part.endswith('**') and len(part) > 4:
            r = p.add_run(part[2:-2])
            set_run_font(r, BODY_FONT, BODY_EA, size, bold=True)
        elif part.startswith('`') and part.endswith('`') and len(part) > 2:
            r = p.add_run(part[1:-1])
            set_run_font(r, MONO, MONO, size - 1)
            r.font.color.rgb = RGBColor(0xC0, 0x30, 0x30)
        elif part.startswith('*') and part.endswith('*') and len(part) > 2:
            r = p.add_run(part[1:-1])
            set_run_font(r, BODY_FONT, BODY_EA, size)
            r.font.italic = True
        else:
            r = p.add_run(part)
            set_run_font(r, BODY_FONT, BODY_EA, size)


def style_heading(doc_para, level):
    sizes = {0: 24, 1: 17, 2: 14, 3: 12}
    size = sizes.get(level, 11)
    for r in doc_para.runs:
        set_run_font(r, HEAD_FONT, HEAD_EA, size, bold=True,
                     color=RGBColor(0x1F, 0x36, 0x64) if level <= 1 else RGBColor(0x22, 0x22, 0x22))


def main():
    raw = open(SRC, encoding='utf-8').read()
    lines = raw.split('\n')

    doc = Document()

    # 页面设置 A4 + 页边距
    sec = doc.sections[0]
    sec.page_width = Cm(21.0)
    sec.page_height = Cm(29.7)
    sec.left_margin = Cm(2.2)
    sec.right_margin = Cm(2.2)
    sec.top_margin = Cm(2.2)
    sec.bottom_margin = Cm(2.0)

    # 正文默认样式
    normal = doc.styles['Normal']
    normal.font.name = BODY_FONT
    normal.font.size = Pt(10.5)
    normal.element.rPr.rFonts.set(qn('w:eastAsia'), BODY_EA)

    # 页脚页码
    fp = sec.footer.paragraphs[0]
    fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = fp.add_run()
    set_run_font(run, BODY_FONT, BODY_EA, 9)
    fld1 = OxmlElement('w:fldChar'); fld1.set(qn('w:fldCharType'), 'begin')
    instr = OxmlElement('w:instrText'); instr.set(qn('xml:space'), 'preserve'); instr.text = 'PAGE'
    fld2 = OxmlElement('w:fldChar'); fld2.set(qn('w:fldCharType'), 'end')
    run._r.append(fld1); run._r.append(instr); run._r.append(fld2)

    i = 0
    n = len(lines)
    first_h1 = True

    while i < n:
        line = lines[i]
        stripped = line.rstrip()

        # ---- 代码块 ----
        if stripped.startswith('```'):
            i += 1
            block = []
            while i < n and not lines[i].strip().startswith('```'):
                block.append(lines[i])
                i += 1
            i += 1
            # 框图 → 渲染成图片，避免中文与框线错位
            if any(c in BOXCHARS for line in block for c in line):
                add_diagram(doc, block)
                continue
            for cl in block:
                p = doc.add_paragraph()
                p.paragraph_format.space_before = Pt(0)
                p.paragraph_format.space_after = Pt(0)
                p.paragraph_format.line_spacing = 1.0
                p.paragraph_format.left_indent = Cm(0.4)
                shade_paragraph(p)
                r = p.add_run(cl if cl.strip() else ' ')
                set_run_font(r, MONO, MONO, 9)
            doc.add_paragraph().paragraph_format.space_after = Pt(4)
            continue

        # ---- 表格 ----
        if stripped.startswith('|') and i + 1 < n and re.match(r'^\s*\|[\s:|-]+\|\s*$', lines[i + 1]):
            header = [c.strip() for c in stripped.strip('|').split('|')]
            i += 2
            rows = []
            while i < n and lines[i].strip().startswith('|'):
                rows.append([c.strip() for c in lines[i].strip().strip('|').split('|')])
                i += 1
            tbl = doc.add_table(rows=1, cols=len(header))
            tbl.style = 'Table Grid'
            tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
            hdr = tbl.rows[0].cells
            for k, h in enumerate(header):
                hdr[k].text = ''
                p = hdr[k].paragraphs[0]
                p.alignment = WD_ALIGN_PARAGRAPH.CENTER
                add_inline(p, '**' + h + '**' if h else '', 9.5)
                shade_paragraph(p, "E8EEF7")
            for row in rows:
                cells = tbl.add_row().cells
                for k in range(len(header)):
                    val = row[k] if k < len(row) else ''
                    cells[k].text = ''
                    p = cells[k].paragraphs[0]
                    add_inline(p, val, 9.5)
            # 表头跨页重复、行不跨页断开
            trPr = tbl.rows[0]._tr.get_or_add_trPr()
            th = OxmlElement('w:tblHeader')
            th.set(qn('w:val'), 'true')
            trPr.append(th)
            for r_ in tbl.rows:
                trPr2 = r_._tr.get_or_add_trPr()
                trPr2.append(OxmlElement('w:cantSplit'))
            doc.add_paragraph().paragraph_format.space_after = Pt(4)
            continue

        # ---- 标题 ----
        m = re.match(r'^(#{1,4})\s+(.*)$', stripped)
        if m:
            level = len(m.group(1))
            text = sanitize(m.group(2).strip())
            if level == 1 and first_h1:
                p = doc.add_paragraph()
                p.alignment = WD_ALIGN_PARAGRAPH.CENTER
                r = p.add_run(text)
                set_run_font(r, HEAD_FONT, HEAD_EA, 26, bold=True,
                             color=RGBColor(0x1F, 0x36, 0x64))
                p.paragraph_format.space_before = Pt(80)
                p.paragraph_format.space_after = Pt(12)
                first_h1 = False
            else:
                p = doc.add_paragraph(style='Heading %d' % min(max(level - 1, 1), 3))
                p.text = ''
                r = p.add_run(text)
                style_heading(p, level - 1)
                p.paragraph_format.space_before = Pt(14 if level <= 2 else 10)
                p.paragraph_format.space_after = Pt(6)
            i += 1
            continue

        # ---- 分割线 ----
        if re.match(r'^\s*---+\s*$', stripped):
            p = doc.add_paragraph()
            pPr = p._p.get_or_add_pPr()
            pbdr = OxmlElement('w:pBdr')
            bottom = OxmlElement('w:bottom')
            bottom.set(qn('w:val'), 'single')
            bottom.set(qn('w:sz'), '6')
            bottom.set(qn('w:color'), 'BBBBBB')
            pbdr.append(bottom)
            pPr.append(pbdr)
            i += 1
            continue

        # ---- 引用 ----
        if stripped.startswith('>'):
            txt = stripped.lstrip('>').strip()
            p = doc.add_paragraph()
            p.paragraph_format.left_indent = Cm(0.8)
            add_inline(p, txt, 10)
            for r in p.runs:
                r.font.color.rgb = RGBColor(0x55, 0x55, 0x55)
            i += 1
            continue

        # ---- 列表 ----
        m = re.match(r'^(\s*)([-*]|\d+\.)\s+(.*)$', stripped)
        if m:
            indent = len(m.group(1)) // 2
            marker = '' if m.group(2)[0].isdigit() else '· '
            p = doc.add_paragraph()
            p.paragraph_format.left_indent = Cm(0.7 + 0.6 * indent)
            p.paragraph_format.space_after = Pt(2)
            if m.group(2)[0].isdigit():
                add_inline(p, m.group(2) + ' ' + m.group(3))
            else:
                add_inline(p, marker + m.group(3))
            i += 1
            continue

        # ---- 空行 ----
        if not stripped.strip():
            i += 1
            continue

        # ---- 普通段落 ----
        p = doc.add_paragraph()
        p.paragraph_format.space_after = Pt(4)
        p.paragraph_format.line_spacing = 1.25
        add_inline(p, stripped)
        i += 1

    doc.save(OUT)
    print("saved:", OUT)


if __name__ == '__main__':
    main()
