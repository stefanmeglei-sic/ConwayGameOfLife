#!/usr/bin/env python3

import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
INPUTS = {
    ("1d", "strong"): ROOT / "coverage" / "bench_1d" / "strong_scaling.csv",
    ("1d", "weak"): ROOT / "coverage" / "bench_1d" / "weak_scaling.csv",
    ("2d", "strong"): ROOT / "coverage" / "bench_2d" / "strong_scaling.csv",
    ("2d", "weak"): ROOT / "coverage" / "bench_2d" / "weak_scaling.csv",
}
OUTPUT_DIR = ROOT / "coverage" / "report_ready"


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="ascii") as handle:
        return list(csv.DictReader(handle))


def as_float(row: dict[str, str], key: str) -> float:
    return float(row[key])


def summarize_strong(rows: list[dict[str, str]], decomposition: str) -> list[dict[str, object]]:
    baseline_total = as_float(rows[0], "total_seconds")
    summary_rows = []
    for row in rows:
        total = as_float(row, "total_seconds")
        communication = as_float(row, "communication_seconds")
        computation = as_float(row, "computation_seconds")
        ranks = int(row["ranks"])
        speedup = baseline_total / total if total else 0.0
        efficiency = speedup / ranks if ranks else 0.0
        communication_pct = (communication / total * 100.0) if total else 0.0
        computation_pct = (computation / total * 100.0) if total else 0.0
        summary_rows.append(
            {
                "decomposition": decomposition,
                "ranks": ranks,
                "width": int(row["width"]),
                "height": int(row["height"]),
                "steps": int(row["steps"]),
                "total_seconds": total,
                "communication_seconds": communication,
                "computation_seconds": computation,
                "speedup": speedup,
                "efficiency": efficiency,
                "communication_percent": communication_pct,
                "computation_percent": computation_pct,
            }
        )
    return summary_rows


def summarize_weak(rows: list[dict[str, str]], decomposition: str) -> list[dict[str, object]]:
    baseline_total = as_float(rows[0], "total_seconds")
    summary_rows = []
    for row in rows:
        total = as_float(row, "total_seconds")
        communication = as_float(row, "communication_seconds")
        computation = as_float(row, "computation_seconds")
        relative_time = total / baseline_total if baseline_total else 0.0
        communication_pct = (communication / total * 100.0) if total else 0.0
        summary_rows.append(
            {
                "decomposition": decomposition,
                "ranks": int(row["ranks"]),
                "width": int(row["width"]),
                "height": int(row["height"]),
                "steps": int(row["steps"]),
                "total_seconds": total,
                "communication_seconds": communication,
                "computation_seconds": computation,
                "relative_time": relative_time,
                "communication_percent": communication_pct,
            }
        )
    return summary_rows


def write_csv(path: Path, rows: list[dict[str, object]], fieldnames: list[str]) -> None:
    with path.open("w", newline="", encoding="ascii") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def format_float(value: object, digits: int = 6) -> str:
    return f"{float(value):.{digits}f}"


def markdown_table(rows: list[dict[str, object]], columns: list[tuple[str, str]]) -> str:
    header = "| " + " | ".join(title for title, _ in columns) + " |"
    separator = "| " + " | ".join("---" for _ in columns) + " |"
    body = []
    for row in rows:
        values = []
        for _, key in columns:
            value = row[key]
            if isinstance(value, float):
                values.append(format_float(value))
            else:
                values.append(str(value))
        body.append("| " + " | ".join(values) + " |")
    return "\n".join([header, separator, *body])


def write_markdown(path: Path,
                   strong_1d: list[dict[str, object]],
                   strong_2d: list[dict[str, object]],
                   weak_1d: list[dict[str, object]],
                   weak_2d: list[dict[str, object]]) -> None:
    strong_columns = [
        ("Ranks", "ranks"),
        ("Total s", "total_seconds"),
        ("Speedup", "speedup"),
        ("Efficiency", "efficiency"),
        ("Comm %", "communication_percent"),
    ]
    weak_columns = [
        ("Ranks", "ranks"),
        ("Grid", "height"),
        ("Total s", "total_seconds"),
        ("Relative Time", "relative_time"),
        ("Comm %", "communication_percent"),
    ]

    lines = [
        "# Benchmark Summary",
        "",
        "Generated from the current CSV datasets under coverage/bench_1d and coverage/bench_2d.",
        "",
        "## Strong Scaling 1D",
        "",
        markdown_table(strong_1d, strong_columns),
        "",
        "## Strong Scaling 2D",
        "",
        markdown_table(strong_2d, strong_columns),
        "",
        "## Weak Scaling 1D",
        "",
        markdown_table(weak_1d, weak_columns),
        "",
        "## Weak Scaling 2D",
        "",
        markdown_table(weak_2d, weak_columns),
        "",
        "## Notes",
        "",
        "- Strong scaling speedup and efficiency are computed relative to the 1-rank run for the same decomposition.",
        "- Weak scaling relative time is computed against the 1-rank run for the same decomposition.",
        "- Communication percent is computed as communication_seconds / total_seconds * 100.",
    ]

    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def main() -> None:
    strong_1d = summarize_strong(read_rows(INPUTS[("1d", "strong")]), "1d")
    strong_2d = summarize_strong(read_rows(INPUTS[("2d", "strong")]), "2d")
    weak_1d = summarize_weak(read_rows(INPUTS[("1d", "weak")]), "1d")
    weak_2d = summarize_weak(read_rows(INPUTS[("2d", "weak")]), "2d")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    write_csv(
        OUTPUT_DIR / "strong_scaling_summary.csv",
        strong_1d + strong_2d,
        [
            "decomposition",
            "ranks",
            "width",
            "height",
            "steps",
            "total_seconds",
            "communication_seconds",
            "computation_seconds",
            "speedup",
            "efficiency",
            "communication_percent",
            "computation_percent",
        ],
    )
    write_csv(
        OUTPUT_DIR / "weak_scaling_summary.csv",
        weak_1d + weak_2d,
        [
            "decomposition",
            "ranks",
            "width",
            "height",
            "steps",
            "total_seconds",
            "communication_seconds",
            "computation_seconds",
            "relative_time",
            "communication_percent",
        ],
    )
    write_markdown(OUTPUT_DIR / "benchmark_summary.md", strong_1d, strong_2d, weak_1d, weak_2d)


if __name__ == "__main__":
    main()