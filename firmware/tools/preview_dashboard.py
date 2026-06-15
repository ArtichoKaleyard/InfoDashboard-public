"""信息看板本地预览器。

用途：
  - 不烧录固件，直接在浏览器里预览 400x300 RLCD 看板布局。
  - 每次刷新都会重新读取 dashboard_layout.h，适合手动改坐标时快速检查。

限制：
  - 这是 SVG/HTML 近似预览，不是完整 LVGL 渲染器。
  - 文本直接读取固件同源 dashboard_font_ui_*.c 位图字体，字形和 advance 与固件保持一致。
  - 坐标、分区、条形图、趋势图和主要文本会跟固件布局保持一致。
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import tempfile
import threading
import time
import urllib.request
from dataclasses import dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
LAYOUT_HEADER = ROOT / "components" / "user_app" / "dashboard_layout.h"
DEFAULT_PORT = 8787
FONT_FILES = {
    10: ROOT / "components" / "user_app" / "dashboard_font_ui_10_1bpp.c",
    12: ROOT / "components" / "user_app" / "dashboard_font_ui_12_1bpp.c",
    14: ROOT / "components" / "user_app" / "dashboard_font_ui_consolas_regular_14_1bpp.c",
    16: ROOT / "components" / "user_app" / "dashboard_font_ui_16m_1bpp.c",
    28: ROOT / "components" / "user_app" / "dashboard_font_ui_28_1bpp.c",
}

JsonObject = dict[str, Any]


@dataclass(frozen=True)
class Rect:
    """对应 dashboard_layout.h 里的 Rect{x, y, w, h}。"""

    x: int
    y: int
    w: int
    h: int


@dataclass(frozen=True)
class Layout:
    """预览器需要的屏幕尺寸和命名矩形。"""

    width: int
    height: int
    rects: dict[str, Rect]


@dataclass(frozen=True)
class Glyph:
    """LVGL 1bpp 字体里的单个 glyph 描述。"""

    bitmap_index: int
    adv_w: int
    box_w: int
    box_h: int
    ofs_x: int
    ofs_y: int


@dataclass(frozen=True)
class BitmapFont:
    """从 dashboard_font_ui_*.c 解析出的 LVGL 位图字体。"""

    size: int
    line_height: int
    base_line: int
    glyph_bitmap: list[int]
    glyphs: list[Glyph]
    range_start: int
    range_length: int
    glyph_id_start: int

    def glyph_for(self, char: str) -> Glyph:
        """按 LVGL FORMAT0_TINY cmap 找 glyph，无法显示时退到问号。"""

        codepoint = ord(char)
        if not self.range_start <= codepoint < self.range_start + self.range_length:
            codepoint = ord("?")
        glyph_id = self.glyph_id_start + codepoint - self.range_start
        return self.glyphs[glyph_id]

    def advance(self, glyph: Glyph) -> int:
        """LVGL 把 8.4 advance 四舍五入成整数像素。"""

        return (glyph.adv_w + 8) >> 4

    def text_width(self, value: str) -> int:
        """计算 LVGL 单行 label 的文本宽度。"""

        return sum(self.advance(self.glyph_for(char)) for char in value)

    def iter_pixels(self, glyph: Glyph) -> list[tuple[int, int]]:
        """返回 glyph 内所有置位像素坐标。"""

        pixels: list[tuple[int, int]] = []
        bit_count = glyph.box_w * glyph.box_h
        for bit_index in range(bit_count):
            byte = self.glyph_bitmap[glyph.bitmap_index + bit_index // 8]
            mask = 0x80 >> (bit_index % 8)
            if byte & mask:
                pixels.append((bit_index % glyph.box_w, bit_index // glyph.box_w))
        return pixels


FONT_CACHE: dict[int, BitmapFont] = {}


def parse_layout(path: Path = LAYOUT_HEADER) -> Layout:
    """解析 dashboard_layout.h 中的 Screen 常量和 Rect 常量。"""

    text = path.read_text(encoding="utf-8")
    width_match = re.search(r"kWidth\s*=\s*(\d+)", text)
    height_match = re.search(r"kHeight\s*=\s*(\d+)", text)
    if not width_match or not height_match:
        raise RuntimeError(f"无法从 {path} 解析屏幕尺寸")

    rects: dict[str, Rect] = {}
    namespace = ""
    for line in text.splitlines():
        namespace_match = re.match(r"namespace\s+(\w+)\s*\{", line.strip())
        if namespace_match:
            namespace = namespace_match.group(1)
            continue
        if line.strip().startswith("}  // namespace"):
            namespace = ""
            continue
        rect_match = re.search(
            r"inline constexpr Rect\s+(\w+)\{(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\}",
            line,
        )
        if rect_match and namespace:
            name, x, y, w, h = rect_match.groups()
            rects[f"{namespace}.{name}"] = Rect(int(x), int(y), int(w), int(h))

    return Layout(int(width_match.group(1)), int(height_match.group(1)), rects)


def parse_bitmap_font(size: int) -> BitmapFont:
    """解析 lv_font_conv 生成的 1bpp LVGL 字体 C 文件。"""

    if size in FONT_CACHE:
        return FONT_CACHE[size]

    path = FONT_FILES[size]
    text = path.read_text(encoding="utf-8")
    bitmap_match = re.search(r"glyph_bitmap\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not bitmap_match:
        raise RuntimeError(f"无法从 {path} 解析 glyph_bitmap")
    bitmap_block = re.sub(r"/\*.*?\*/", "", bitmap_match.group(1), flags=re.S)
    glyph_bitmap = [int(value, 0) for value in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", bitmap_block)]

    glyph_block_match = re.search(r"glyph_dsc\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not glyph_block_match:
        raise RuntimeError(f"无法从 {path} 解析 glyph_dsc")
    glyphs = [
        Glyph(*(int(value) for value in match))
        for match in re.findall(
            r"\.bitmap_index\s*=\s*(\d+),\s*\.adv_w\s*=\s*(\d+),\s*\.box_w\s*=\s*(\d+),\s*"
            r"\.box_h\s*=\s*(\d+),\s*\.ofs_x\s*=\s*(-?\d+),\s*\.ofs_y\s*=\s*(-?\d+)",
            glyph_block_match.group(1),
        )
    ]
    range_match = re.search(
        r"\.range_start\s*=\s*(\d+),\s*\.range_length\s*=\s*(\d+),\s*\.glyph_id_start\s*=\s*(\d+)",
        text,
    )
    line_match = re.search(r"\.line_height\s*=\s*(\d+)", text)
    base_match = re.search(r"\.base_line\s*=\s*(\d+)", text)
    if not range_match or not line_match or not base_match:
        raise RuntimeError(f"无法从 {path} 解析字体元数据")

    font = BitmapFont(
        size=size,
        line_height=int(line_match.group(1)),
        base_line=int(base_match.group(1)),
        glyph_bitmap=glyph_bitmap,
        glyphs=glyphs,
        range_start=int(range_match.group(1)),
        range_length=int(range_match.group(2)),
        glyph_id_start=int(range_match.group(3)),
    )
    FONT_CACHE[size] = font
    return font


def font_for_size(size: int) -> BitmapFont:
    """选择固件实际存在的字号。"""

    if size in FONT_FILES:
        return parse_bitmap_font(size)
    nearest_size = min(FONT_FILES, key=lambda item: abs(item - size))
    return parse_bitmap_font(nearest_size)


def default_snapshot() -> JsonObject:
    """返回一份偏真实的默认展示数据。"""

    return {
        "schema": 1,
        "updated_at": "2026-06-05T19:45:00+08:00",
        "weather": {
            "location": "Local",
            "condition": "Cloudy",
            "temp_c": 28.1,
            "humidity_pct": 45,
            "wind_kmh": 18.9,
            "state": "ok",
        },
        "server_monitor": {
            "target": "ATK-AHU",
            "role": "CHILD NODE",
            "state": "ONLINE",
            "summary": "2 GPU procs active",
            "cpu_percent": 23,
            "memory_percent": 61,
            "latency_ms": 18,
            "gpu_util_percent": 72,
            "gpu_memory_percent": 76,
            "gpu_temp_c": 62,
            "gpu_processes": 2,
        },
        "codex": {
            "state": "good",
            "source": "configured_api",
            "weekly_remaining_pct": 70,
            "five_hour_remaining_pct": 55,
            "trend_pct": [0, 0, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            "trend_active_indexes": [2],
            "weekly_reset_at": "WEEK THU 16:00",
            "five_hour_reset_at": "5H TODAY 20:41",
            "updated_at": "2026-06-05T19:45:00+08:00",
        },
        "local": {
            "time": "19:45:08",
            "date": "Fri 2026-06-05",
            "indoor": "26.2C 54 %",
            "battery": "Battery USB",
            "link": "WIFI OK",
        },
    }


def load_snapshot(args: argparse.Namespace) -> JsonObject:
    """读取 JSON 文件、bridge URL 或默认样例。"""

    if args.json:
        return json.loads(Path(args.json).read_text(encoding="utf-8"))
    if args.url:
        with urllib.request.urlopen(args.url, timeout=args.timeout) as response:
            payload = json.loads(response.read().decode("utf-8"))
        if not isinstance(payload, dict):
            raise RuntimeError("bridge 返回的 JSON 不是对象")
        return payload
    return default_snapshot()


class Svg:
    """简易 SVG 生成器。"""

    def __init__(self, width: int, height: int) -> None:
        self.width = width
        self.height = height
        self.parts: list[str] = []

    def rect(self, rect: Rect, fill: str = "none", stroke: str = "#000", stroke_width: int = 0, radius: int = 0) -> None:
        self.parts.append(
            f'<rect x="{rect.x}" y="{rect.y}" width="{rect.w}" height="{rect.h}" '
            f'rx="{radius}" ry="{radius}" fill="{fill}" stroke="{stroke}" stroke-width="{stroke_width}"/>'
        )

    def text(
        self,
        rect: Rect,
        value: str,
        size: int,
        color: str = "#000",
        anchor: str = "start",
        weight: int = 600,
        letter_space: int = 0,
    ) -> None:
        """用固件同源 LVGL 1bpp 字体绘制文本。"""

        del weight
        text = str(value)
        font = font_for_size(size)
        text_width = font.text_width(text) + max(0, len(text) - 1) * letter_space
        cursor_x = rect.x
        if anchor == "middle":
            cursor_x = rect.x + max(0, (rect.w - text_width) // 2)
        elif anchor == "end":
            cursor_x = rect.x + rect.w - text_width

        clip_x1 = rect.x
        clip_y1 = rect.y
        clip_x2 = rect.x + rect.w
        clip_y2 = rect.y + rect.h
        baseline_y = rect.y + font.line_height - font.base_line
        pixels: list[str] = []
        for index, char in enumerate(text):
            glyph = font.glyph_for(char)
            glyph_x = cursor_x + glyph.ofs_x
            glyph_y = baseline_y - glyph.box_h - glyph.ofs_y
            for pixel_x, pixel_y in font.iter_pixels(glyph):
                x = glyph_x + pixel_x
                y = glyph_y + pixel_y
                if clip_x1 <= x < clip_x2 and clip_y1 <= y < clip_y2:
                    pixels.append(f'<rect x="{x}" y="{y}" width="1" height="1"/>')
            cursor_x += font.advance(glyph)
            if index < len(text) - 1:
                cursor_x += letter_space
        if pixels:
            self.parts.append(f'<g fill="{color}">{"".join(pixels)}</g>')

    def line(self, x1: int, y1: int, x2: int, y2: int, dash: bool = False) -> None:
        dash_attr = ' stroke-dasharray="4 3"' if dash else ""
        self.parts.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#000" stroke-width="1"{dash_attr}/>')

    def finish(self) -> str:
        body = "\n".join(self.parts)
        return (
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.width} {self.height}" '
            f'width="{self.width}" height="{self.height}" shape-rendering="crispEdges">'
            f'<rect width="{self.width}" height="{self.height}" fill="#fff"/>{body}</svg>'
        )


def short_source(value: str) -> str:
    """对应固件 CopyCodexSource() 的短显示。"""

    if value == "configured_api":
        return "OAuth"
    if value == "status_file":
        return "File"
    return value[:8] if value else "Config"


def last_token(value: str) -> str:
    """取 reset 文本最后一个 token。"""

    return value.split()[-1] if value else "--"


def reset_detail(value: str) -> str:
    """Remove the quota-window prefix while preserving the useful reset time."""

    value = value.strip()
    for prefix in ("WEEK ", "W ", "5H ", "5-HOUR "):
        if value.startswith(prefix):
            return value[len(prefix) :]
    return value or "--"


def draw_panel(svg: Svg, layout: Layout, key: str) -> Rect:
    """画主卡片外框并返回卡片矩形。"""

    panel = layout.rects[key]
    svg.rect(panel, fill="#fff", stroke="#000", stroke_width=2, radius=4)
    return panel


def inner(panel: Rect, rect: Rect) -> Rect:
    """把卡片内坐标转换成屏幕坐标。"""

    return Rect(panel.x + rect.x, panel.y + rect.y, rect.w, rect.h)


def draw_fill(svg: Svg, rect: Rect, fill: str = "#000", radius: int = 0) -> None:
    """画无边框填充块。"""

    svg.rect(rect, fill=fill, stroke=fill, stroke_width=0, radius=radius)


def draw_bar(svg: Svg, rect: Rect, percent: int) -> None:
    """画和固件类似的空心进度条。"""

    percent = max(0, min(100, int(percent)))
    svg.rect(rect, fill="none", stroke="#000", stroke_width=1, radius=2)
    fill_width = max(0, (rect.w - 2) * percent // 100)
    if fill_width:
        draw_fill(svg, Rect(rect.x + 1, rect.y + 1, fill_width, rect.h - 2), radius=1)


def draw_trend(svg: Svg, area: Rect, values: list[int], active_indexes: list[int]) -> None:
    """画 Codex 12 段趋势柱。"""

    gap = 7
    bar_w = 4
    start_x = area.x + (area.w - (12 * bar_w + 11 * gap)) // 2
    for index in range(12):
        value = max(0, min(100, int(values[index] if index < len(values) else 0)))
        if value <= 0:
            continue
        height = max(3, area.h * value // 100)
        x = start_x + index * (bar_w + gap)
        y = area.y + area.h - height
        fill = "#000" if index in active_indexes else "#777"
        draw_fill(svg, Rect(x, y, bar_w, height), fill=fill, radius=2)


def render_dashboard(snapshot: JsonObject, layout: Layout) -> str:
    """按当前布局和快照渲染 SVG。"""

    svg = Svg(layout.width, layout.height)
    rects = layout.rects
    local = snapshot.get("local", {})
    weather = snapshot.get("weather", {})
    server = snapshot.get("server_monitor", {})
    codex = snapshot.get("codex", {})

    draw_fill(svg, rects["Header.kBar"])
    svg.text(rects["Header.kTitle"], "OPS BOARD", 14, "#fff", weight=700)
    svg.text(rects["Header.kDate"], str(local.get("date", "Fri 2026-06-05")), 14, "#fff", anchor="middle")
    svg.text(rects["Header.kTime"], str(local.get("time", "19:45:08")), 14, "#fff", anchor="end")
    svg.text(rects["Header.kLinkState"], str(local.get("link", "OK")).replace("WIFI ", ""), 14, "#fff", anchor="end")

    server_panel = draw_panel(svg, layout, "Server.kPanel")
    draw_fill(svg, inner(server_panel, rects["Server.kHeader"]))
    svg.text(inner(server_panel, rects["Server.kTitle"]), "SERVERMONITOR", 14, "#fff", weight=700)
    svg.text(inner(server_panel, rects["Server.kTarget"]), str(server.get("target", "ATK-AHU")), 14, "#fff", "end", 700)
    svg.text(inner(server_panel, rects["Server.kHeroLabel"]), "GPU UTIL", 14, weight=600)
    svg.text(inner(server_panel, rects["Server.kGpuUtil"]), f'{int(server.get("gpu_util_percent", 0))} %', 28, weight=500)
    badge = inner(server_panel, rects["Server.kStateBadge"])
    draw_fill(svg, badge, radius=2)
    svg.text(Rect(badge.x + 3, badge.y + 2, badge.w - 6, badge.h - 4), str(server.get("state", "ONLINE")), 14, "#fff", "middle")
    svg.text(inner(server_panel, rects["Server.kRole"]), str(server.get("role", "CHILD")), 14, anchor="end")
    svg.text(inner(server_panel, rects["Server.kSummary"]), str(server.get("summary", "2 GPU procs active")), 14)
    for key in ("Server.kDividerTop", "Server.kDividerBottom"):
        r = inner(server_panel, rects[key])
        svg.line(r.x, r.y, r.x + r.w, r.y, dash=True)
    svg.text(inner(server_panel, rects["Server.kGpuBarLabel"]), "GPU", 14)
    draw_bar(svg, inner(server_panel, rects["Server.kGpuBar"]), int(server.get("gpu_util_percent", 0)))
    svg.text(inner(server_panel, rects["Server.kGpuBarPct"]), f'{int(server.get("gpu_util_percent", 0))} %', 14, anchor="end")
    svg.text(inner(server_panel, rects["Server.kVramBarLabel"]), "VRAM", 14)
    draw_bar(svg, inner(server_panel, rects["Server.kVramBar"]), int(server.get("gpu_memory_percent", 0)))
    svg.text(inner(server_panel, rects["Server.kVramBarPct"]), f'{int(server.get("gpu_memory_percent", 0))} %', 14, anchor="end")
    stat_map = [
        ("Tmp", "Server.kTmpLabel", f'{int(server.get("gpu_temp_c", 0))}C', "Server.kTmpValue"),
        ("Png", "Server.kPingLabel", f'{int(server.get("latency_ms", 0))}ms', "Server.kPingValue"),
        ("CPU", "Server.kCpuLabel", f'{int(server.get("cpu_percent", 0))} %', "Server.kCpuValue"),
        ("MEM", "Server.kMemLabel", f'{int(server.get("memory_percent", 0))} %', "Server.kMemValue"),
    ]
    for label, label_key, value, value_key in stat_map:
        svg.text(inner(server_panel, rects[label_key]), label, 12, weight=600)
        svg.text(inner(server_panel, rects[value_key]), value, 12, anchor="end", weight=700)

    codex_panel = draw_panel(svg, layout, "Codex.kPanel")
    draw_fill(svg, inner(codex_panel, rects["Codex.kHeader"]))
    svg.text(inner(codex_panel, rects["Codex.kTitle"]), "Codex Pro 5x", 14, "#fff", weight=600)
    svg.text(inner(codex_panel, rects["Codex.kState"]), str(codex.get("state", "good")).title(), 14, "#fff", "end", 700)
    svg.text(inner(codex_panel, rects["Codex.kTrendTitle"]), "Use Trend", 14)
    svg.text(inner(codex_panel, rects["Codex.kTrendRange"]), "7Day", 14, anchor="end")
    draw_trend(
        svg,
        inner(codex_panel, rects["Codex.kTrendArea"]),
        list(codex.get("weekly_trend_pct", codex.get("trend_pct", []))),
        list(codex.get("weekly_trend_active_indexes", codex.get("trend_active_indexes", []))),
    )
    svg.text(inner(codex_panel, rects["Codex.kTrendMarkLeft"]), "Start", 10)
    svg.text(inner(codex_panel, rects["Codex.kTrendMarkRight"]), "Reset", 10, anchor="end")
    for key in ("Codex.kDividerTop", "Codex.kDividerBottom"):
        r = inner(codex_panel, rects[key])
        svg.line(r.x, r.y, r.x + r.w, r.y, dash=True)
    svg.text(inner(codex_panel, rects["Codex.kWeekLabel"]), "7Day", 14)
    draw_bar(svg, inner(codex_panel, rects["Codex.kWeekBar"]), int(codex.get("weekly_remaining_pct", 0)))
    svg.text(inner(codex_panel, rects["Codex.kWeekPct"]), f'{int(codex.get("weekly_remaining_pct", 0))} %', 14, anchor="end")
    svg.text(inner(codex_panel, rects["Codex.kFiveHourLabel"]), "5hour", 14)
    draw_bar(svg, inner(codex_panel, rects["Codex.kFiveHourBar"]), int(codex.get("five_hour_remaining_pct", 0)))
    svg.text(inner(codex_panel, rects["Codex.kFiveHourPct"]), f'{int(codex.get("five_hour_remaining_pct", 0))} %', 14, anchor="end")
    svg.text(inner(codex_panel, rects["Codex.kRateLabel"]), "Src", 10)
    svg.text(inner(codex_panel, rects["Codex.kRateValue"]), short_source(str(codex.get("source", ""))), 10, anchor="end")
    svg.text(inner(codex_panel, rects["Codex.kWeekResetLabel"]), "W RST", 10)
    svg.text(inner(codex_panel, rects["Codex.kWeekResetValue"]), reset_detail(str(codex.get("weekly_reset_at", ""))), 10, anchor="end")
    svg.text(inner(codex_panel, rects["Codex.kFiveHourResetLabel"]), "5H RST", 10)
    svg.text(inner(codex_panel, rects["Codex.kFiveHourResetValue"]), reset_detail(str(codex.get("five_hour_reset_at", ""))), 10, anchor="end")

    draw_fill(svg, rects["Aux.kAuxBarTop"])
    svg.text(rects["Aux.kWeatherTitle"], "Weather", 14)
    weather_text = f'{weather.get("location", "Local")} {float(weather.get("temp_c", 0)):.1f}C {weather.get("condition", "Unknown")}'
    svg.text(rects["Aux.kWeatherValue"], weather_text, 14)
    draw_fill(svg, rects["Aux.kAuxDivider"])
    svg.text(rects["Aux.kIndoorTitle"], "Indoor", 14)
    svg.text(rects["Aux.kIndoorValue"], str(local.get("indoor", "26.2C 54 %")), 12, anchor="end")
    draw_fill(svg, rects["Aux.kFooterBar"], radius=3)
    svg.text(rects["Aux.kFooterUpdated"], "Updated 19:45", 10, "#fff")
    svg.text(rects["Aux.kFooterBattery"], str(local.get("battery", "Battery USB")), 10, "#fff", "middle")
    svg.text(rects["Aux.kFooterLink"], f'Link {local.get("link", "WIFI OK")}', 10, "#fff", "end")
    return svg.finish()


def draw_rule_x(svg: Svg, x: int, y: int, width: int) -> None:
    """Draw a horizontal dashed separator."""

    svg.line(x, y, x + width, y, dash=True)


def draw_rule_y(svg: Svg, x: int, y: int, height: int) -> None:
    """Draw a vertical dashed separator."""

    svg.line(x, y, x, y + height, dash=True)


def draw_zebra_bar(svg: Svg, rect: Rect, percent: int) -> None:
    """Draw a 1-bit striped quota/progress bar like the HTML reference."""

    percent = max(0, min(100, int(percent)))
    svg.rect(rect, fill="none", stroke="#000", stroke_width=1, radius=2)
    fill_width = max(0, (rect.w - 2) * percent // 100)
    x = rect.x + 2
    end = rect.x + 1 + fill_width
    while x < end:
        draw_fill(svg, Rect(x, rect.y + 2, 2, rect.h - 4), radius=1)
        x += 4


def draw_led_row(svg: Svg, rect: Rect, percent: int) -> None:
    """Draw nine GPU load LEDs with only the row's outer corners rounded."""

    percent = max(0, min(100, int(percent)))
    count = 9
    lit_count = max(0, min(count, (count * percent + 99) // 100))
    gap = 2
    dot_w = max(3, (rect.w - gap * (count - 1)) // count)
    for index in range(count):
        x = rect.x + index * (dot_w + gap)
        fill = "#000" if index < lit_count else "#fff"
        svg.rect(Rect(x, rect.y, dot_w, rect.h), fill=fill, stroke="#000", stroke_width=1)
        if index == 0:
            svg.rect(Rect(x, rect.y, 1, 1), fill="#fff")
            svg.rect(Rect(x, rect.y + rect.h - 1, 1, 1), fill="#fff")
        elif index == count - 1:
            svg.rect(Rect(x + dot_w - 1, rect.y, 1, 1), fill="#fff")
            svg.rect(Rect(x + dot_w - 1, rect.y + rect.h - 1, 1, 1), fill="#fff")


def draw_mini_chart(svg: Svg, rect: Rect, values: list[int], count: int) -> None:
    """Draw a compressed bar chart."""

    gap = 6
    bar_w = 5
    total_w = bar_w * count + gap * (count - 1)
    start_x = rect.x + (rect.w - total_w) // 2
    baseline = rect.y + rect.h
    budget_y = baseline - (rect.h * 2 + 1) // 3
    dash_w = 2
    dash_gap = 16
    for x in range(rect.x, rect.x + rect.w, dash_w + dash_gap):
        svg.rect(Rect(x, budget_y, min(dash_w, rect.x + rect.w - x), 1), fill="#000")
    for index in range(count):
        value = max(0, min(100, int(values[index] if index < len(values) else 0)))
        height = 1 if value == 0 else max(3, rect.h * value // 100)
        x = start_x + index * (bar_w + gap)
        y = baseline - height
        svg.rect(Rect(x, y, bar_w, height), fill="#000", radius=2)


def gpu_card_list(server: JsonObject) -> list[JsonObject]:
    """Return two GPU card dictionaries, falling back to aggregate values."""

    cards = [card for card in list(server.get("gpu_cards") or []) if isinstance(card, dict)]
    if not cards:
        cards = [
            {
                "label": "GPU0",
                "util_percent": int(server.get("gpu_util_percent", 0)),
                "memory_percent": int(server.get("gpu_memory_percent", 0)),
                "temp_c": int(server.get("gpu_temp_c", 0)),
                "processes": int(server.get("gpu_processes", 0)),
            }
        ]
    while len(cards) < 2:
        cards.append({"label": f"GPU{len(cards)}", "util_percent": 0, "memory_percent": 0, "temp_c": 0, "processes": 0})
    return cards[:2]


def format_indoor_text(local: JsonObject) -> str:
    """Return compact indoor temperature and humidity text."""

    raw = str(local.get("indoor", "28.2C / 31%")).strip()
    raw = raw.replace("INDOOR:", "").replace("Indoor:", "").strip()
    if "/" in raw:
        return f"INDOOR: {raw}".upper()
    parts = [part for part in raw.replace("%", "% ").split() if part]
    if len(parts) >= 2:
        return f"INDOOR: {parts[0]} / {parts[1]}".upper()
    return f"INDOOR: {raw}".upper()


def render_dashboard(snapshot: JsonObject, layout: Layout) -> str:
    """Render the hardware HUD reference layout from ops_board_layout_fixed_v2."""

    rects = layout.rects
    svg = Svg(layout.width, layout.height)
    local = snapshot.get("local", {})
    weather = snapshot.get("weather", {})
    server = snapshot.get("server_monitor", {})
    codex = snapshot.get("codex", {})
    cards = gpu_card_list(server)

    svg.rect(Rect(0, 0, layout.width, layout.height), fill="#fff", stroke="#000", stroke_width=1)
    draw_fill(svg, rects["Header.kBar"])
    svg.text(rects["Header.kTitle"], "> SYS.OP_BOARD", 12, "#fff")
    svg.text(rects["Header.kDate"], str(local.get("date", "FRI_2026.06.05")).replace(" ", "_").upper(), 12, "#fff", "middle")
    svg.text(rects["Header.kTime"], str(local.get("time", "19:53:56")), 14, "#fff", "end")

    left = rects["Server.kPanel"]
    right = rects["Codex.kPanel"]
    svg.rect(left, fill="#fff", stroke="#000", stroke_width=1, radius=4)
    draw_fill(svg, inner(left, rects["Server.kHeaderRule"]))
    svg.text(inner(left, rects["Server.kTarget"]), "ServerMonitor", 16, weight=600)
    badge_rect = inner(left, rects["Server.kStateBadge"])
    draw_fill(svg, badge_rect, radius=2)
    svg.text(Rect(badge_rect.x + 2, badge_rect.y + 2, badge_rect.w - 4, 10), str(server.get("state", "ONLINE")).upper()[:6], 10, "#fff", "middle")

    svg.rect(right, fill="#fff", stroke="#000", stroke_width=1, radius=4)
    draw_fill(svg, inner(right, rects["Codex.kHeaderRule"]))
    svg.text(inner(right, rects["Codex.kTitle"]), "Codex Pro 5x", 16, weight=600)
    badge_rect = inner(right, rects["Codex.kState"])
    draw_fill(svg, badge_rect, radius=2)
    svg.text(Rect(badge_rect.x + 2, badge_rect.y + 2, badge_rect.w - 4, 10), str(codex.get("state", "GOOD")).upper()[:6], 10, "#fff", "middle")

    gpu_label_rects = [rects["Server.kGpu0Label"], rects["Server.kGpu1Label"]]
    gpu_temp_rects = [rects["Server.kGpu0Temp"], rects["Server.kGpu1Temp"]]
    gpu_util_rects = [rects["Server.kGpu0Util"], rects["Server.kGpu1Util"]]
    gpu_pct_rects = [rects["Server.kGpu0Pct"], rects["Server.kGpu1Pct"]]
    gpu_cell_rects = [rects["Server.kGpu0Cells"], rects["Server.kGpu1Cells"]]
    for idx, card in enumerate(cards):
        if idx == 1:
            divider = inner(left, rects["Server.kGpuDividerY"])
            draw_rule_y(svg, divider.x, divider.y, divider.h)
        label = str(card.get("label", f"GPU{idx}")).replace("GPU", "GPU.")
        util = int(card.get("util_percent", 0))
        temp = int(card.get("temp_c", 0))
        svg.text(inner(left, gpu_label_rects[idx]), label.upper(), 10)
        svg.text(inner(left, gpu_temp_rects[idx]), f"{temp}C", 10, anchor="end")
        svg.text(inner(left, gpu_util_rects[idx]), f"{util}", 28, anchor="end")
        svg.text(inner(left, gpu_pct_rects[idx]), "%", 14)
        draw_led_row(svg, inner(left, gpu_cell_rects[idx]), util)
    divider = inner(left, rects["Server.kDividerTop"])
    draw_rule_x(svg, divider.x, divider.y, divider.w)

    vram_label_rects = [rects["Server.kVram0Label"], rects["Server.kVram1Label"]]
    vram_pct_rects = [rects["Server.kVram0Pct"], rects["Server.kVram1Pct"]]
    vram_bar_rects = [rects["Server.kVram0Bar"], rects["Server.kVram1Bar"]]
    for row, card in enumerate(cards):
        mem = int(card.get("memory_percent", 0))
        label = str(card.get("label", f"GPU{row}")).replace("GPU", "VRAM.GPU")
        svg.text(inner(left, vram_label_rects[row]), label.upper(), 10)
        svg.text(inner(left, vram_pct_rects[row]), f"{mem}%", 10, anchor="end")
        draw_zebra_bar(svg, inner(left, vram_bar_rects[row]), mem)
    divider = inner(left, rects["Server.kDividerBottom"])
    draw_rule_x(svg, divider.x, divider.y, divider.w)

    p0 = int(cards[0].get("power_w", 0) or 0)
    p1 = int(cards[1].get("power_w", 0) or 0)
    proc = int(server.get("gpu_processes", 0))
    svg.text(inner(left, rects["Server.kPower0"]), f"P.GPU0 {p0 or '--'}W", 12)
    svg.text(inner(left, rects["Server.kPower1"]), f"P.GPU1 {p1 or '--'}W", 12, anchor="end")
    svg.text(inner(left, rects["Server.kCpu"]), f"CPU {int(server.get('cpu_percent', 0))}%", 12)
    svg.text(inner(left, rects["Server.kMem"]), f"MEM {int(server.get('memory_percent', 0))}%", 12)
    svg.text(inner(left, rects["Server.kProc"]), f"PROC {proc:02d}", 12, anchor="end")

    week_values = [20, 35, 28, 80, 34, 15, 38]
    week_trend = list(codex.get("weekly_trend_pct", codex.get("trend_pct", [])))
    if week_trend:
        week_values = [int(value) for value in week_trend[:7]]
    five_values = [78, 70, 86, 55, 35]
    five_trend = list(codex.get("five_hour_trend_pct", []))
    if five_trend:
        five_values = [int(value) for value in five_trend[:5]]
    svg.text(inner(right, rects["Codex.kTrend0Label"]), "7_DAY", 10)
    draw_mini_chart(svg, inner(right, rects["Codex.kTrend0Chart"]), week_values, 7)
    divider = inner(right, rects["Codex.kTrendDividerY"])
    draw_rule_y(svg, divider.x, divider.y, divider.h)
    svg.text(inner(right, rects["Codex.kTrend1Label"]), "5_HR", 10)
    draw_mini_chart(svg, inner(right, rects["Codex.kTrend1Chart"]), five_values, 5)
    divider = inner(right, rects["Codex.kDividerTop"])
    draw_rule_x(svg, divider.x, divider.y, divider.w)

    week_pct = int(codex.get("weekly_remaining_pct", 0))
    five_hour_pct = int(codex.get("five_hour_remaining_pct", 0))
    svg.text(inner(right, rects["Codex.kWeekLabel"]), "Q.WEEK", 10)
    svg.text(inner(right, rects["Codex.kWeekPct"]), f"{week_pct}%", 10, anchor="end")
    draw_zebra_bar(svg, inner(right, rects["Codex.kWeekBar"]), week_pct)
    svg.text(inner(right, rects["Codex.kFiveHourLabel"]), "Q.5_HR", 10)
    svg.text(inner(right, rects["Codex.kFiveHourPct"]), f"{five_hour_pct}%", 10, anchor="end")
    draw_zebra_bar(svg, inner(right, rects["Codex.kFiveHourBar"]), five_hour_pct)
    divider = inner(right, rects["Codex.kDividerBottom"])
    draw_rule_x(svg, divider.x, divider.y, divider.w)
    svg.text(inner(right, rects["Codex.kWeekResetLabel"]), "WEEK RESET", 12)
    svg.text(inner(right, rects["Codex.kWeekResetValue"]), reset_detail(str(codex.get("weekly_reset_at", "WEEK THU 16:00"))), 12, anchor="end")
    svg.text(inner(right, rects["Codex.kFiveHourResetLabel"]), "5H RESET", 12)
    svg.text(inner(right, rects["Codex.kFiveHourResetValue"]), reset_detail(str(codex.get("five_hour_reset_at", "5H TODAY 20:41"))), 12, anchor="end")

    draw_fill(svg, rects["Aux.kAuxRule"])
    weather_text = f"> Weather: {weather.get('location', 'LOCAL')} {float(weather.get('temp_c', 0)):.1f}C / {weather.get('condition', 'Cloudy')}"
    svg.text(rects["Aux.kWeatherValue"], weather_text, 12)
    svg.text(rects["Aux.kIndoorValue"], format_indoor_text(local), 12, anchor="end")
    draw_fill(svg, rects["Aux.kFooterBar"], radius=3)
    svg.text(rects["Aux.kFooterLink"], f"> Link: [ {local.get('link', 'WIFI_OK').replace(' ', '_')} ]", 12, "#fff")
    svg.text(rects["Aux.kFooterBattery"], str(local.get("battery", "Battery 56%")).replace("BATT: ", "Battery "), 12, "#fff", anchor="middle")
    svg.text(rects["Aux.kFooterUpdated"], "Updated 19:53", 12, "#fff", anchor="end")
    return svg.finish()


def render_page(args: argparse.Namespace) -> str:
    """渲染带说明文字的 HTML 页面。"""

    svg = render_dashboard(load_snapshot(args), parse_layout())
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 信息看板预览</title>
<style>
body {{
  margin: 0;
  min-height: 100vh;
  display: grid;
  place-items: center;
  background: #202020;
  color: #e8e8e8;
  font-family: system-ui, "Microsoft YaHei", sans-serif;
}}
.wrap {{ padding: 24px; }}
.screen {{
  width: 800px;
  max-width: calc(100vw - 32px);
  aspect-ratio: 4 / 3;
  background: #fff;
  box-shadow: 0 18px 50px rgba(0,0,0,.55);
  image-rendering: pixelated;
}}
.screen svg {{ width: 100%; height: 100%; display: block; }}
.note {{ max-width: 800px; margin-top: 12px; font-size: 13px; color: #bbb; }}
code {{ color: #fff; }}
</style>
</head>
<body>
<main class="wrap">
  <div class="screen">{svg}</div>
  <div class="note">刷新页面会重新读取 <code>dashboard_layout.h</code> 和 <code>dashboard_font_ui_*.c</code>。这是 SVG 近似预览，不模拟 RLCD 物理显示和刷新残影。</div>
</main>
</body>
</html>"""


def render_export_page(args: argparse.Namespace, scale: int = 2) -> str:
    """渲染只包含屏幕本体的 HTML，供浏览器截图导出 PNG。"""

    layout = parse_layout()
    svg = render_dashboard(load_snapshot(args), layout)
    width = layout.width * scale
    height = layout.height * scale
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<style>
html, body {{
  margin: 0;
  width: {width}px;
  height: {height}px;
  overflow: hidden;
  background: #fff;
}}
svg {{
  width: {width}px;
  height: {height}px;
  display: block;
}}
</style>
</head>
<body>{svg}</body>
</html>"""


def write_text(path: Path, content: str) -> None:
    """写出 UTF-8 文本，并自动创建父目录。"""

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def export_svg(args: argparse.Namespace, path: Path) -> None:
    """导出当前预览为 SVG。"""

    write_text(path, render_dashboard(load_snapshot(args), parse_layout()))
    print(f"已导出 SVG：{path}")


def export_png(args: argparse.Namespace, path: Path) -> None:
    """导出当前预览为 PNG。

    优先使用 resvg-js 直接把 SVG 渲染成 PNG；浏览器截图只作为兜底。
    """

    layout = parse_layout()
    if try_export_png_with_resvg(args, path, layout):
        print(f"已导出 PNG：{path}")
        return
    if try_export_png_with_playwright(args, path, layout):
        print(f"已导出 PNG：{path}")
        return
    if try_export_png_with_browser(args, path, layout):
        print(f"已导出 PNG：{path}")
        return
    raise RuntimeError("导出 PNG 需要 resvg-js，或 Playwright / Edge / Chrome headless。当前环境未找到可用导出器。")


def try_export_png_with_resvg(args: argparse.Namespace, path: Path, layout: Layout) -> bool:
    """尝试使用 resvg-js CLI 把 SVG 转成 PNG。"""

    converter = find_resvg_converter()
    if not converter:
        return False

    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dashboard-preview-", ignore_cleanup_errors=True) as temp_dir:
        svg_path = Path(temp_dir) / "screen.svg"
        write_text(svg_path, render_dashboard(load_snapshot(args), layout))
        command = [
            str(converter),
            "--fit-width",
            str(layout.width),
            "--background",
            "#ffffff",
            str(svg_path),
            str(path.resolve()),
        ]
        completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=30)
        return completed.returncode == 0 and path.exists()


def try_export_png_with_playwright(args: argparse.Namespace, path: Path, layout: Layout) -> bool:
    """尝试使用 Python Playwright 导出 PNG。"""

    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        return False

    page_path = path.with_suffix(".preview.html")
    write_text(page_path, render_export_page(args))
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch()
            page = browser.new_page(
                viewport={"width": layout.width * 2, "height": layout.height * 2},
                device_scale_factor=1,
            )
            page.goto(page_path.resolve().as_uri())
            page.screenshot(path=str(path))
            browser.close()
    finally:
        try:
            page_path.unlink()
        except OSError:
            pass
    return True


def try_export_png_with_browser(args: argparse.Namespace, path: Path, layout: Layout) -> bool:
    """尝试使用 Edge/Chrome headless 导出 PNG。"""

    browser = find_headless_browser()
    if not browser:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dashboard-preview-", ignore_cleanup_errors=True) as temp_dir:
        temp_root = Path(temp_dir)
        profile_path = temp_root / "profile"
        export_html = render_export_page(args)
        server = make_one_page_server(export_html)
        thread = threading.Thread(target=server.serve_forever, name="png-export-server", daemon=True)
        thread.start()
        host, port = server.server_address
        url = f"http://{host}:{port}/"
        wait_for_http_server(url)
        command = [
            str(browser),
            "--headless=new",
            "--disable-gpu",
            "--no-first-run",
            "--disable-extensions",
            f"--user-data-dir={profile_path}",
            f"--window-size={layout.width * 2},{layout.height * 2}",
            f"--screenshot={path.resolve()}",
            url,
        ]
        try:
            completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=30)
            return completed.returncode == 0 and path.exists()
        finally:
            server.shutdown()
            server.server_close()


def make_one_page_server(content: str) -> ThreadingHTTPServer:
    """创建只服务一个 HTML 页面的一次性本地 HTTP server。"""

    class OnePageHandler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            body = content.encode("utf-8")
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt: str, *args: Any) -> None:
            return

    return ThreadingHTTPServer(("localhost", 0), OnePageHandler)


def wait_for_http_server(url: str) -> None:
    """等待一次性预览 server 就绪。"""

    deadline = time.monotonic() + 5
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(url, timeout=0.5):
                return
        except OSError as exc:
            last_error = exc
            time.sleep(0.05)
    raise RuntimeError(f"临时预览 server 未能启动：{last_error}")


def find_headless_browser() -> Path | None:
    """查找 Windows 上常见的 Edge/Chrome 可执行文件。"""

    command = shutil.which("msedge") or shutil.which("chrome") or shutil.which("chromium")
    if command:
        return Path(command)
    candidates = [
        Path("C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"),
        Path("C:/Program Files/Microsoft/Edge/Application/msedge.exe"),
        Path("C:/Program Files/Google/Chrome/Application/chrome.exe"),
        Path("C:/Program Files (x86)/Google/Chrome/Application/chrome.exe"),
    ]
    return next((candidate for candidate in candidates if candidate.exists()), None)


def find_resvg_converter() -> Path | None:
    """查找 resvg-js CLI。"""

    for name in ("resvg-js.cmd", "resvg-js", "resvg-js-cli.cmd", "resvg-js-cli"):
        command = shutil.which(name)
        if command:
            return Path(command)
    return None


class PreviewHandler(BaseHTTPRequestHandler):
    """本地预览 HTTP handler。"""

    args: argparse.Namespace

    def do_GET(self) -> None:
        """返回 HTML 页面或 SVG 画面。"""

        if self.path.split("?", 1)[0] == "/screen.svg":
            body = render_dashboard(load_snapshot(self.args), parse_layout()).encode("utf-8")
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "image/svg+xml; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        body = render_page(self.args).encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt: str, *args: Any) -> None:
        """保持终端输出简洁。"""

        print(f"{self.address_string()} - {fmt % args}")


def parse_args() -> argparse.Namespace:
    """解析命令行参数。"""

    parser = argparse.ArgumentParser(description="本地预览 ESP32 信息看板 UI")
    parser.add_argument("--host", default="localhost", help="预览服务监听地址")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="预览服务端口")
    parser.add_argument("--json", help="从 JSON 文件读取 dashboard payload")
    parser.add_argument("--url", help="从 bridge URL 读取 dashboard payload")
    parser.add_argument("--timeout", type=float, default=8.0, help="读取 bridge URL 的超时时间")
    parser.add_argument("--export-svg", type=Path, help="直接导出 SVG 后退出")
    parser.add_argument("--export-png", type=Path, help="直接导出 PNG 后退出，优先使用 resvg-js")
    return parser.parse_args()


def main() -> int:
    """启动预览服务。"""

    args = parse_args()
    if args.export_svg:
        export_svg(args, args.export_svg)
    if args.export_png:
        export_png(args, args.export_png)
    if args.export_svg or args.export_png:
        return 0

    PreviewHandler.args = args
    server = ThreadingHTTPServer((args.host, args.port), PreviewHandler)
    print(f"信息看板预览：http://{args.host}:{args.port}")
    print("改 dashboard_layout.h 后刷新浏览器即可。按 Ctrl+C 退出。")
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
