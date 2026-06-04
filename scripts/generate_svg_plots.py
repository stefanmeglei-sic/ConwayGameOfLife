#!/usr/bin/env python3

import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
STRONG = ROOT / "coverage" / "report_ready" / "strong_scaling_summary.csv"
WEAK = ROOT / "coverage" / "report_ready" / "weak_scaling_summary.csv"
OUT_DIR = ROOT / "coverage" / "report_ready" / "plots"


def read_csv(path: Path):
    with path.open("r", encoding="ascii", newline="") as handle:
        return list(csv.DictReader(handle))


def split_by_decomposition(rows):
    buckets = {"1d": [], "2d": []}
    for row in rows:
        buckets[row["decomposition"]].append(row)
    for key in buckets:
        buckets[key].sort(key=lambda r: int(r["ranks"]))
    return buckets


def line_chart_svg(title, x_label, y_label, x_values, series, y_min=None, y_max=None):
    width = 920
    height = 560
    left = 70
    right = 30
    top = 60
    bottom = 70
    chart_w = width - left - right
    chart_h = height - top - bottom

    if y_min is None:
        y_min = min(min(values) for _, _, values in series)
    if y_max is None:
        y_max = max(max(values) for _, _, values in series)
    if y_max <= y_min:
        y_max = y_min + 1.0

    x_min = min(x_values)
    x_max = max(x_values)
    if x_max <= x_min:
        x_max = x_min + 1

    def x_to_px(x):
        return left + (x - x_min) / (x_max - x_min) * chart_w

    def y_to_px(y):
        return top + (1.0 - (y - y_min) / (y_max - y_min)) * chart_h

    lines = []
    lines.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">')
    lines.append('<rect x="0" y="0" width="100%" height="100%" fill="white"/>')
    lines.append(f'<text x="{width/2}" y="30" text-anchor="middle" font-size="22" font-family="sans-serif">{title}</text>')

    # Axes
    lines.append(f'<line x1="{left}" y1="{top + chart_h}" x2="{left + chart_w}" y2="{top + chart_h}" stroke="#222" stroke-width="2"/>')
    lines.append(f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + chart_h}" stroke="#222" stroke-width="2"/>')

    # Grid + y ticks
    for i in range(6):
        value = y_min + (y_max - y_min) * i / 5
        y = y_to_px(value)
        lines.append(f'<line x1="{left}" y1="{y}" x2="{left + chart_w}" y2="{y}" stroke="#ddd" stroke-width="1"/>')
        lines.append(f'<text x="{left - 10}" y="{y + 5}" text-anchor="end" font-size="12" font-family="sans-serif">{value:.2f}</text>')

    # x ticks
    for x in x_values:
        px = x_to_px(x)
        lines.append(f'<line x1="{px}" y1="{top + chart_h}" x2="{px}" y2="{top + chart_h + 6}" stroke="#222" stroke-width="1"/>')
        lines.append(f'<text x="{px}" y="{top + chart_h + 24}" text-anchor="middle" font-size="12" font-family="sans-serif">{x}</text>')

    # Labels
    lines.append(f'<text x="{width/2}" y="{height - 20}" text-anchor="middle" font-size="14" font-family="sans-serif">{x_label}</text>')
    lines.append(f'<text x="20" y="{height/2}" text-anchor="middle" font-size="14" transform="rotate(-90 20 {height/2})" font-family="sans-serif">{y_label}</text>')

    # Series
    legend_x = left + chart_w - 190
    legend_y = top + 10
    for idx, (name, color, values) in enumerate(series):
        points = " ".join(f"{x_to_px(x_values[i]):.2f},{y_to_px(values[i]):.2f}" for i in range(len(x_values)))
        lines.append(f'<polyline fill="none" stroke="{color}" stroke-width="3" points="{points}"/>')
        for i, x in enumerate(x_values):
            lines.append(f'<circle cx="{x_to_px(x):.2f}" cy="{y_to_px(values[i]):.2f}" r="4" fill="{color}"/>')

        ly = legend_y + idx * 24
        lines.append(f'<line x1="{legend_x}" y1="{ly}" x2="{legend_x + 24}" y2="{ly}" stroke="{color}" stroke-width="3"/>')
        lines.append(f'<text x="{legend_x + 32}" y="{ly + 4}" font-size="13" font-family="sans-serif">{name}</text>')

    lines.append("</svg>")
    return "\n".join(lines) + "\n"


def write_svg(path: Path, content: str):
    path.write_text(content, encoding="ascii")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    strong_rows = split_by_decomposition(read_csv(STRONG))
    weak_rows = split_by_decomposition(read_csv(WEAK))

    ranks = [int(row["ranks"]) for row in strong_rows["1d"]]

    strong_speedup = line_chart_svg(
        title="Strong Scaling Speedup",
        x_label="MPI Ranks",
        y_label="Speedup",
        x_values=ranks,
        series=[
            ("1D", "#e66101", [float(r["speedup"]) for r in strong_rows["1d"]]),
            ("2D", "#5e3c99", [float(r["speedup"]) for r in strong_rows["2d"]]),
        ],
        y_min=0.0,
    )
    write_svg(OUT_DIR / "strong_speedup.svg", strong_speedup)

    strong_eff = line_chart_svg(
        title="Strong Scaling Efficiency",
        x_label="MPI Ranks",
        y_label="Efficiency",
        x_values=ranks,
        series=[
            ("1D", "#fdb863", [float(r["efficiency"]) for r in strong_rows["1d"]]),
            ("2D", "#b2abd2", [float(r["efficiency"]) for r in strong_rows["2d"]]),
        ],
        y_min=0.0,
        y_max=1.05,
    )
    write_svg(OUT_DIR / "strong_efficiency.svg", strong_eff)

    strong_comm_compute = line_chart_svg(
        title="Strong Scaling: Communication vs Computation",
        x_label="MPI Ranks",
        y_label="Percent of Total Time",
        x_values=ranks,
        series=[
            ("1D Comm %", "#1b9e77", [float(r["communication_percent"]) for r in strong_rows["1d"]]),
            ("1D Comp %", "#66a61e", [float(r["computation_percent"]) for r in strong_rows["1d"]]),
            ("2D Comm %", "#d95f02", [float(r["communication_percent"]) for r in strong_rows["2d"]]),
            ("2D Comp %", "#7570b3", [float(r["computation_percent"]) for r in strong_rows["2d"]]),
        ],
        y_min=0.0,
        y_max=100.0,
    )
    write_svg(OUT_DIR / "strong_comm_vs_compute.svg", strong_comm_compute)

    weak_ranks = [int(row["ranks"]) for row in weak_rows["1d"]]

    weak_relative = line_chart_svg(
        title="Weak Scaling Relative Time",
        x_label="MPI Ranks",
        y_label="Relative Time",
        x_values=weak_ranks,
        series=[
            ("1D", "#e7298a", [float(r["relative_time"]) for r in weak_rows["1d"]]),
            ("2D", "#1f78b4", [float(r["relative_time"]) for r in weak_rows["2d"]]),
        ],
        y_min=0.0,
    )
    write_svg(OUT_DIR / "weak_relative_time.svg", weak_relative)

    weak_comm = line_chart_svg(
        title="Weak Scaling Communication Share",
        x_label="MPI Ranks",
        y_label="Communication Percent",
        x_values=weak_ranks,
        series=[
            ("1D Comm %", "#a6761d", [float(r["communication_percent"]) for r in weak_rows["1d"]]),
            ("2D Comm %", "#666666", [float(r["communication_percent"]) for r in weak_rows["2d"]]),
        ],
        y_min=0.0,
    )
    write_svg(OUT_DIR / "weak_communication_percent.svg", weak_comm)


if __name__ == "__main__":
    main()