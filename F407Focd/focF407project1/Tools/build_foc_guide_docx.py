from pathlib import Path
import re

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "FOC_FROM_ZERO_GUIDE.md"
OUTPUT = ROOT / "FOC从零到电机转动说明书.docx"

FONT_CN = "Microsoft YaHei"
FONT_CODE = "Consolas"
BLUE = "2E74B5"
DARK_BLUE = "1F4D78"
INK = "243447"
MUTED = "687386"
LIGHT_BLUE = "E8EEF5"
LIGHT_GRAY = "F4F6F9"
CAUTION = "FFF3CD"
RED = "9B1C1C"
TABLE_WIDTH_DXA = 9360
TABLE_INDENT_DXA = 120


def set_run_font(run, name=FONT_CN, size=None, color=None, bold=None, italic=None):
    run.font.name = name
    run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), name)
    run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), name)
    run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), name)
    if size is not None:
        run.font.size = Pt(size)
    if color is not None:
        run.font.color.rgb = RGBColor.from_string(color)
    if bold is not None:
        run.bold = bold
    if italic is not None:
        run.italic = italic


def set_cell_shading(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top=80, start=120, bottom=80, end=120):
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for name, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{name}"))
        if node is None:
            node = OxmlElement(f"w:{name}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def set_repeat_table_header(row):
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def set_table_geometry(table, widths):
    table.autofit = False
    table.alignment = WD_TABLE_ALIGNMENT.LEFT
    tbl_pr = table._tbl.tblPr
    tbl_w = tbl_pr.find(qn("w:tblW"))
    if tbl_w is None:
        tbl_w = OxmlElement("w:tblW")
        tbl_pr.append(tbl_w)
    tbl_w.set(qn("w:w"), str(sum(widths)))
    tbl_w.set(qn("w:type"), "dxa")

    tbl_ind = tbl_pr.find(qn("w:tblInd"))
    if tbl_ind is None:
        tbl_ind = OxmlElement("w:tblInd")
        tbl_pr.append(tbl_ind)
    tbl_ind.set(qn("w:w"), str(TABLE_INDENT_DXA))
    tbl_ind.set(qn("w:type"), "dxa")

    grid = table._tbl.tblGrid
    for child in list(grid):
        grid.remove(child)
    for width in widths:
        col = OxmlElement("w:gridCol")
        col.set(qn("w:w"), str(width))
        grid.append(col)

    for row in table.rows:
        for idx, cell in enumerate(row.cells):
            width = widths[idx]
            tc_pr = cell._tc.get_or_add_tcPr()
            tc_w = tc_pr.find(qn("w:tcW"))
            if tc_w is None:
                tc_w = OxmlElement("w:tcW")
                tc_pr.append(tc_w)
            tc_w.set(qn("w:w"), str(width))
            tc_w.set(qn("w:type"), "dxa")
            set_cell_margins(cell)
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER


def add_page_number(paragraph):
    paragraph.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    run = paragraph.add_run("第 ")
    set_run_font(run, size=9, color=MUTED)
    fld_char1 = OxmlElement("w:fldChar")
    fld_char1.set(qn("w:fldCharType"), "begin")
    instr_text = OxmlElement("w:instrText")
    instr_text.set(qn("xml:space"), "preserve")
    instr_text.text = "PAGE"
    fld_char2 = OxmlElement("w:fldChar")
    fld_char2.set(qn("w:fldCharType"), "end")
    run._r.extend([fld_char1, instr_text, fld_char2])
    run2 = paragraph.add_run(" 页")
    set_run_font(run2, size=9, color=MUTED)


def create_decimal_numbering(doc):
    numbering = doc.part.numbering_part.element
    abstract_ids = [int(node.get(qn("w:abstractNumId"))) for node in numbering.findall(qn("w:abstractNum"))]
    num_ids = [int(node.get(qn("w:numId"))) for node in numbering.findall(qn("w:num"))]
    abstract_id = max(abstract_ids, default=-1) + 1
    num_id = max(num_ids, default=0) + 1

    abstract = OxmlElement("w:abstractNum")
    abstract.set(qn("w:abstractNumId"), str(abstract_id))
    multi = OxmlElement("w:multiLevelType")
    multi.set(qn("w:val"), "singleLevel")
    abstract.append(multi)

    level = OxmlElement("w:lvl")
    level.set(qn("w:ilvl"), "0")
    start = OxmlElement("w:start")
    start.set(qn("w:val"), "1")
    level.append(start)
    num_fmt = OxmlElement("w:numFmt")
    num_fmt.set(qn("w:val"), "decimal")
    level.append(num_fmt)
    lvl_text = OxmlElement("w:lvlText")
    lvl_text.set(qn("w:val"), "%1.")
    level.append(lvl_text)
    suff = OxmlElement("w:suff")
    suff.set(qn("w:val"), "tab")
    level.append(suff)

    p_pr = OxmlElement("w:pPr")
    tabs = OxmlElement("w:tabs")
    tab = OxmlElement("w:tab")
    tab.set(qn("w:val"), "num")
    tab.set(qn("w:pos"), "540")
    tabs.append(tab)
    p_pr.append(tabs)
    ind = OxmlElement("w:ind")
    ind.set(qn("w:left"), "540")
    ind.set(qn("w:hanging"), "270")
    p_pr.append(ind)
    spacing = OxmlElement("w:spacing")
    spacing.set(qn("w:after"), "80")
    spacing.set(qn("w:line"), "300")
    spacing.set(qn("w:lineRule"), "auto")
    p_pr.append(spacing)
    level.append(p_pr)
    abstract.append(level)
    first_num_index = len(numbering)
    for index, child in enumerate(numbering):
        if child.tag == qn("w:num"):
            first_num_index = index
            break
    numbering.insert(first_num_index, abstract)

    num = OxmlElement("w:num")
    num.set(qn("w:numId"), str(num_id))
    abstract_ref = OxmlElement("w:abstractNumId")
    abstract_ref.set(qn("w:val"), str(abstract_id))
    num.append(abstract_ref)
    level_override = OxmlElement("w:lvlOverride")
    level_override.set(qn("w:ilvl"), "0")
    start_override = OxmlElement("w:startOverride")
    start_override.set(qn("w:val"), "1")
    level_override.append(start_override)
    num.append(level_override)
    numbering.append(num)
    return num_id


def apply_numbering(paragraph, num_id):
    p_pr = paragraph._p.get_or_add_pPr()
    num_pr = p_pr.find(qn("w:numPr"))
    if num_pr is None:
        num_pr = OxmlElement("w:numPr")
        p_pr.append(num_pr)
    ilvl = OxmlElement("w:ilvl")
    ilvl.set(qn("w:val"), "0")
    num_id_node = OxmlElement("w:numId")
    num_id_node.set(qn("w:val"), str(num_id))
    num_pr.append(ilvl)
    num_pr.append(num_id_node)


def configure_styles(doc):
    styles = doc.styles
    normal = styles["Normal"]
    normal.font.name = FONT_CN
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), FONT_CN)
    normal.font.size = Pt(11)
    normal.font.color.rgb = RGBColor.from_string(INK)
    normal.paragraph_format.space_before = Pt(0)
    normal.paragraph_format.space_after = Pt(6)
    normal.paragraph_format.line_spacing = 1.25

    heading_tokens = {
        "Heading 1": (16, BLUE, 18, 10),
        "Heading 2": (13, BLUE, 14, 7),
        "Heading 3": (12, DARK_BLUE, 10, 5),
    }
    for style_name, (size, color, before, after) in heading_tokens.items():
        style = styles[style_name]
        style.font.name = FONT_CN
        style._element.rPr.rFonts.set(qn("w:eastAsia"), FONT_CN)
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = RGBColor.from_string(color)
        style.paragraph_format.space_before = Pt(before)
        style.paragraph_format.space_after = Pt(after)
        style.paragraph_format.keep_with_next = True

    for style_name in ("List Bullet", "List Number"):
        style = styles[style_name]
        style.font.name = FONT_CN
        style._element.rPr.rFonts.set(qn("w:eastAsia"), FONT_CN)
        style.font.size = Pt(11)
        style.paragraph_format.left_indent = Inches(0.375)
        style.paragraph_format.first_line_indent = Inches(-0.188)
        style.paragraph_format.space_after = Pt(4)
        style.paragraph_format.line_spacing = 1.25

    if "Code Block" not in styles:
        code_style = styles.add_style("Code Block", WD_STYLE_TYPE.PARAGRAPH)
    else:
        code_style = styles["Code Block"]
    code_style.font.name = FONT_CODE
    code_style._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
    code_style.font.size = Pt(9)
    code_style.font.color.rgb = RGBColor.from_string("263238")
    code_style.paragraph_format.left_indent = Inches(0.15)
    code_style.paragraph_format.right_indent = Inches(0.15)
    code_style.paragraph_format.space_before = Pt(3)
    code_style.paragraph_format.space_after = Pt(6)
    code_style.paragraph_format.line_spacing = 1.05

    if "Lead Callout" not in styles:
        callout = styles.add_style("Lead Callout", WD_STYLE_TYPE.PARAGRAPH)
    else:
        callout = styles["Lead Callout"]
    callout.font.name = FONT_CN
    callout._element.rPr.rFonts.set(qn("w:eastAsia"), FONT_CN)
    callout.font.size = Pt(11)
    callout.font.bold = True
    callout.font.color.rgb = RGBColor.from_string(DARK_BLUE)
    callout.paragraph_format.left_indent = Inches(0.18)
    callout.paragraph_format.right_indent = Inches(0.18)
    callout.paragraph_format.space_before = Pt(6)
    callout.paragraph_format.space_after = Pt(8)
    callout.paragraph_format.line_spacing = 1.25


def shade_paragraph(paragraph, fill, border_color=None):
    p_pr = paragraph._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), fill)
    p_pr.append(shd)
    if border_color:
        p_bdr = OxmlElement("w:pBdr")
        left = OxmlElement("w:left")
        left.set(qn("w:val"), "single")
        left.set(qn("w:sz"), "16")
        left.set(qn("w:space"), "8")
        left.set(qn("w:color"), border_color)
        p_bdr.append(left)
        p_pr.append(p_bdr)


def add_inline_runs(paragraph, text, default_size=11):
    pattern = re.compile(r"(\*\*.+?\*\*|`.+?`)")
    cursor = 0
    for match in pattern.finditer(text):
        if match.start() > cursor:
            run = paragraph.add_run(text[cursor:match.start()])
            set_run_font(run, size=default_size, color=INK)
        token = match.group(0)
        if token.startswith("**"):
            run = paragraph.add_run(token[2:-2])
            set_run_font(run, size=default_size, color=INK, bold=True)
        else:
            run = paragraph.add_run(token[1:-1])
            set_run_font(run, name=FONT_CODE, size=default_size - 0.5, color=DARK_BLUE)
        cursor = match.end()
    if cursor < len(text):
        run = paragraph.add_run(text[cursor:])
        set_run_font(run, size=default_size, color=INK)


def add_code_block(doc, lines):
    for idx, text in enumerate(lines):
        p = doc.add_paragraph(style="Code Block")
        p.paragraph_format.keep_together = True
        run = p.add_run(text if text else " ")
        set_run_font(run, name=FONT_CODE, size=9, color="263238")
        shade_paragraph(p, LIGHT_GRAY, DARK_BLUE if idx == 0 else None)


def add_markdown_table(doc, rows):
    if len(rows) < 2:
        return
    header = rows[0]
    body = rows[2:] if all(re.fullmatch(r":?-{3,}:?", cell.strip()) for cell in rows[1]) else rows[1:]
    col_count = len(header)
    table = doc.add_table(rows=1, cols=col_count)
    table.style = "Table Grid"
    total_chars = [max(4, len(header[i])) for i in range(col_count)]
    for row in body:
        for i, cell in enumerate(row[:col_count]):
            total_chars[i] = max(total_chars[i], min(len(cell), 28))
    total = sum(total_chars)
    widths = [max(1000, round(TABLE_WIDTH_DXA * value / total)) for value in total_chars]
    widths[-1] += TABLE_WIDTH_DXA - sum(widths)

    for idx, text in enumerate(header):
        cell = table.rows[0].cells[idx]
        cell.text = ""
        p = cell.paragraphs[0]
        add_inline_runs(p, text, 9.5)
        for run in p.runs:
            run.bold = True
            run.font.color.rgb = RGBColor.from_string(DARK_BLUE)
        set_cell_shading(cell, LIGHT_BLUE)
    set_repeat_table_header(table.rows[0])

    for row_data in body:
        cells = table.add_row().cells
        for idx in range(col_count):
            text = row_data[idx] if idx < len(row_data) else ""
            cells[idx].text = ""
            p = cells[idx].paragraphs[0]
            p.paragraph_format.space_after = Pt(0)
            p.paragraph_format.line_spacing = 1.12
            add_inline_runs(p, text, 9.5)
    set_table_geometry(table, widths)
    doc.add_paragraph().paragraph_format.space_after = Pt(1)


def parse_table_row(line):
    return [part.strip() for part in line.strip().strip("|").split("|")]


def add_cover(doc):
    section = doc.sections[0]
    section.page_width = Inches(8.5)
    section.page_height = Inches(11)
    section.top_margin = Inches(1)
    section.bottom_margin = Inches(1)
    section.left_margin = Inches(1)
    section.right_margin = Inches(1)
    section.header_distance = Inches(0.492)
    section.footer_distance = Inches(0.492)

    for _ in range(5):
        doc.add_paragraph()
    kicker = doc.add_paragraph()
    kicker.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = kicker.add_run("STM32 FOC 技术操作手册")
    set_run_font(run, size=11, color=BLUE, bold=True)
    kicker.paragraph_format.space_after = Pt(18)

    title = doc.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    title.paragraph_format.space_after = Pt(10)
    run = title.add_run("从零开始让PMSM电机转起来")
    set_run_font(run, size=28, color=DARK_BLUE, bold=True)

    subtitle = doc.add_paragraph()
    subtitle.alignment = WD_ALIGN_PARAGRAPH.CENTER
    subtitle.paragraph_format.space_after = Pt(28)
    run = subtitle.add_run("FOC原理、逐级调试与故障排查说明书")
    set_run_font(run, size=15, color=BLUE)

    rule = doc.add_paragraph()
    rule.paragraph_format.space_after = Pt(26)
    p_pr = rule._p.get_or_add_pPr()
    p_bdr = OxmlElement("w:pBdr")
    bottom = OxmlElement("w:bottom")
    bottom.set(qn("w:val"), "single")
    bottom.set(qn("w:sz"), "18")
    bottom.set(qn("w:space"), "1")
    bottom.set(qn("w:color"), BLUE)
    p_bdr.append(bottom)
    p_pr.append(p_bdr)

    info = [
        "项目：focF407project1",
        "硬件：STM32F407 + 灯哥开源FOC V3 PLUS + MT6701",
        "对象：只懂C语言和单片机基础框架的初学者",
        "版本：2026-11-09",
    ]
    for text in info:
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.space_after = Pt(6)
        run = p.add_run(text)
        set_run_font(run, size=10.5, color=MUTED)

    for _ in range(3):
        doc.add_paragraph()
    warning = doc.add_paragraph(style="Lead Callout")
    warning.alignment = WD_ALIGN_PARAGRAPH.CENTER
    warning.add_run("安全提示：方向、角度和电流采样未确认前，不要直接使用大电流测试。")
    shade_paragraph(warning, CAUTION, "7A5A00")

    doc.add_page_break()


def add_quick_navigation(doc):
    p = doc.add_paragraph()
    p.style = "Heading 1"
    p.add_run("使用导读")
    lead = doc.add_paragraph(style="Lead Callout")
    lead.add_run("先完成当前关卡，再进入下一关。任何一级未通过，都不要靠加大电流或乱调PID跳过去。")
    shade_paragraph(lead, LIGHT_BLUE, BLUE)

    stages = [
        ("第1关", "PWM零输出", "确认零命令不出力"),
        ("第2关", "只读编码器", "确认一圈计数与机械方向"),
        ("第3关", "极对数", "确认机械角到电角度倍率"),
        ("第4关", "静态偏移", "对齐编码器零点与转子磁极"),
        ("第5关", "低压开环", "验证旋转磁场与动态角度关系"),
        ("第6关", "电流环", "让iq成为可控转矩"),
        ("第7关", "速度环", "让iq自动调节速度"),
        ("第8关", "位置环", "让速度目标自动追踪位置"),
    ]
    table = doc.add_table(rows=1, cols=3)
    table.style = "Table Grid"
    for idx, text in enumerate(("阶段", "项目", "通过目标")):
        cell = table.rows[0].cells[idx]
        cell.text = text
        set_cell_shading(cell, LIGHT_BLUE)
        for run in cell.paragraphs[0].runs:
            set_run_font(run, size=9.5, color=DARK_BLUE, bold=True)
    for stage, name, goal in stages:
        cells = table.add_row().cells
        for idx, text in enumerate((stage, name, goal)):
            cells[idx].text = text
            for run in cells[idx].paragraphs[0].runs:
                set_run_font(run, size=9.5, color=INK)
    set_repeat_table_header(table.rows[0])
    set_table_geometry(table, [1100, 2200, 6060])
    doc.add_page_break()


def add_body_from_markdown(doc, markdown):
    lines = markdown.splitlines()
    index = 0
    in_code = False
    code_lines = []
    table_rows = []
    skipped_first_title = False
    active_numbering_id = None

    def flush_table():
        nonlocal table_rows
        if table_rows:
            add_markdown_table(doc, table_rows)
            table_rows = []

    while index < len(lines):
        line = lines[index].rstrip()

        if line.startswith("```"):
            flush_table()
            if in_code:
                add_code_block(doc, code_lines)
                code_lines = []
                in_code = False
            else:
                in_code = True
            index += 1
            continue
        if in_code:
            code_lines.append(line)
            index += 1
            continue

        if line.startswith("|") and line.endswith("|"):
            table_rows.append(parse_table_row(line))
            index += 1
            continue
        flush_table()

        if not line or line == "---":
            active_numbering_id = None
            index += 1
            continue
        if line.startswith("# "):
            if not skipped_first_title:
                skipped_first_title = True
            index += 1
            continue
        if line.startswith("> "):
            index += 1
            continue
        if line.startswith("## "):
            active_numbering_id = None
            text = line[3:].strip()
            p = doc.add_paragraph(style="Heading 1")
            add_inline_runs(p, text, 16)
            for run in p.runs:
                run.bold = True
                run.font.color.rgb = RGBColor.from_string(BLUE)
            index += 1
            continue
        if line.startswith("### "):
            active_numbering_id = None
            text = line[4:].strip()
            p = doc.add_paragraph(style="Heading 2")
            add_inline_runs(p, text, 13)
            for run in p.runs:
                run.bold = True
                run.font.color.rgb = RGBColor.from_string(BLUE)
            index += 1
            continue
        if re.match(r"^\d+\.\s+", line):
            text = re.sub(r"^\d+\.\s+", "", line)
            if active_numbering_id is None:
                active_numbering_id = create_decimal_numbering(doc)
            p = doc.add_paragraph()
            p.paragraph_format.space_after = Pt(4)
            p.paragraph_format.line_spacing = 1.25
            apply_numbering(p, active_numbering_id)
            add_inline_runs(p, text)
            index += 1
            continue
        if line.startswith("- [ ] "):
            active_numbering_id = None
            p = doc.add_paragraph(style="List Bullet")
            add_inline_runs(p, "[ ] " + line[6:])
            index += 1
            continue
        if line.startswith("- "):
            active_numbering_id = None
            p = doc.add_paragraph(style="List Bullet")
            add_inline_runs(p, line[2:])
            index += 1
            continue

        p = doc.add_paragraph()
        active_numbering_id = None
        add_inline_runs(p, line)
        if line.startswith("不要") or "立即断" in line or "安全" in line[:8]:
            shade_paragraph(p, CAUTION, "7A5A00")
        index += 1
    flush_table()
    if code_lines:
        add_code_block(doc, code_lines)


def configure_header_footer(doc):
    for section in doc.sections:
        header = section.header
        hp = header.paragraphs[0]
        hp.alignment = WD_ALIGN_PARAGRAPH.LEFT
        hp.paragraph_format.space_after = Pt(0)
        run = hp.add_run("FOC从零到电机转动说明书  |  focF407project1")
        set_run_font(run, size=8.5, color=MUTED)
        footer = section.footer
        fp = footer.paragraphs[0]
        add_page_number(fp)


def main():
    markdown = SOURCE.read_text(encoding="utf-8")
    doc = Document()
    configure_styles(doc)
    add_cover(doc)
    add_quick_navigation(doc)
    add_body_from_markdown(doc, markdown)
    configure_header_footer(doc)

    props = doc.core_properties
    props.title = "从零开始让PMSM电机转起来：FOC原理、逐级调试与故障排查说明书"
    props.subject = "STM32F407 FOC初学者操作手册"
    props.author = "focF407project1"
    props.keywords = "FOC, PMSM, STM32F407, MT6701, 电流环, 速度环, 位置环"

    doc.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    main()
