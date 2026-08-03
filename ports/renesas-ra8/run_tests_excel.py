#!/usr/bin/env python3
"""Run MicroPython tests on a serial target and create an Excel report."""

import argparse
import datetime
import itertools
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import zipfile
from xml.sax.saxutils import escape


REPO_ROOT = Path(__file__).resolve().parents[2]
TESTS_DIR = REPO_ROOT / "tests"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run tests, retry each failed test, and write an .xlsx report."
    )
    parser.add_argument("-t", "--device", required=True, help="serial port, for example COM10")
    parser.add_argument("-b", "--baudrate", type=int, default=2_000_000)
    parser.add_argument(
        "-d",
        "--test-dirs",
        nargs="+",
        default=["basics"],
        help="directories below tests/ (default: basics)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="report path (default: micropython-test-report-YYYYMMDD-HHMMSS.xlsx)",
    )
    parser.add_argument("--python", default=sys.executable, help="Python executable")
    parser.add_argument(
        "--run-tests-arg",
        action="append",
        default=[],
        help="extra run-tests.py argument; repeat this option when needed",
    )
    parser.add_argument(
        "--keep-artifacts", action="store_true", help="keep logs and raw results in %%TEMP%%"
    )
    return parser.parse_args()


def run_command(command, result_dir, log_file):
    result_dir.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    with log_file.open("w", encoding="utf-8", errors="replace") as log:
        process = subprocess.Popen(
            command,
            cwd=TESTS_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        assert process.stdout is not None
        for line in process.stdout:
            log.write(line)
        return_code = process.wait()
    return return_code, round(time.monotonic() - started, 2)


def load_results(result_dir, required=True):
    results_file = result_dir / "_results.json"
    if not results_file.is_file():
        if required:
            raise RuntimeError("run-tests.py did not create {}".format(results_file))
        return None
    with results_file.open(encoding="utf-8") as source:
        return json.load(source)["results"]


def make_base_command(args, result_dir):
    return [
        args.python,
        "run-tests.py",
        "-t",
        args.device,
        "-b",
        str(args.baudrate),
        "--result-dir",
        str(result_dir),
        *args.run_tests_arg,
    ]


def normalise_test_name(name):
    path = Path(name)
    try:
        return path.resolve().relative_to(TESTS_DIR.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def find_unittest_candidates(test_dir):
    directory = TESTS_DIR / test_dir
    candidates = []
    if not directory.is_dir():
        return candidates

    unittest_pattern = re.compile(r"^\s*(?:import unittest|from unittest\b)", re.MULTILINE)
    for test_file in sorted(directory.glob("*.py")):
        source = test_file.read_text(encoding="utf-8", errors="replace")
        if unittest_pattern.search(source):
            candidates.append(test_file.relative_to(TESTS_DIR).as_posix())
    return candidates


def find_target_wiring_candidates(test_dir):
    directory = TESTS_DIR / test_dir
    candidates = []
    if not directory.is_dir():
        return candidates

    target_wiring_pattern = re.compile(
        r"^\s*(?:import target_wiring|from target_wiring\b)", re.MULTILINE
    )
    for test_file in sorted(directory.glob("*.py")):
        source = test_file.read_text(encoding="utf-8", errors="replace")
        if target_wiring_pattern.search(source):
            candidates.append(test_file.relative_to(TESTS_DIR).as_posix())
    return candidates


def log_requires_unittest(log_file):
    output = log_file.read_text(encoding="utf-8", errors="replace")
    return (
        "requires unittest" in output
        or "ImportError: no module named 'unittest'" in output
    )


def exclude_pattern(test_names):
    alternatives = "|".join(re.escape(name) for name in test_names)
    return r"(?:^|/)(?:{})$".format(alternatives)


def column_name(number):
    name = ""
    while number:
        number, remainder = divmod(number - 1, 26)
        name = chr(65 + remainder) + name
    return name


def worksheet_xml(rows, widths, freeze=True):
    xml_rows = []
    for row_number, row in enumerate(rows, 1):
        cells = []
        for column_number, value in enumerate(row, 1):
            reference = "{}{}".format(column_name(column_number), row_number)
            style = ' s="1"' if row_number == 1 else ""
            if isinstance(value, (int, float)) and not isinstance(value, bool):
                cells.append('<c r="{}"{}><v>{}</v></c>'.format(reference, style, value))
            else:
                text = escape("" if value is None else str(value))
                cells.append(
                    '<c r="{}"{} t="inlineStr"><is><t xml:space="preserve">{}</t></is></c>'.format(
                        reference, style, text
                    )
                )
        xml_rows.append('<row r="{}">{}</row>'.format(row_number, "".join(cells)))
    columns = "".join(
        '<col min="{0}" max="{0}" width="{1}" customWidth="1"/>'.format(index, width)
        for index, width in enumerate(widths, 1)
    )
    pane = '<pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/>' if freeze else ""
    auto_filter = '<autoFilter ref="A1:{}{}"/>'.format(column_name(len(widths)), len(rows)) if rows else ""
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
        '<sheetViews><sheetView workbookViewId="0">{}</sheetView></sheetViews>'
        '<cols>{}</cols><sheetData>{}</sheetData>{}</worksheet>'.format(
            pane, columns, "".join(xml_rows), auto_filter
        )
    )


def worksheet_title(test_dir, used_titles):
    title = "{}测试明细".format(test_dir.replace("/", "-"))[:31]
    original_title = title
    suffix = 2
    while title in used_titles:
        suffix_text = "-{}".format(suffix)
        title = "{}{}".format(original_title[: 31 - len(suffix_text)], suffix_text)
        suffix += 1
    used_titles.add(title)
    return title


def write_xlsx(output, summary_rows, detail_sheets):
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    worksheets = [("测试汇总", summary_rows, [22, 16, 16, 22, 16, 16, 16, 16, 16])]
    worksheets.extend(
        (title, rows, [58, 72, 58, 72]) for title, rows in detail_sheets
    )
    worksheet_content_types = "".join(
        '<Override PartName="/xl/worksheets/sheet{}.xml" '
        'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'.format(
            index
        )
        for index in range(1, len(worksheets) + 1)
    )
    content_types = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>{}</Types>""".format(worksheet_content_types)
    root_rels = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>"""
    workbook_sheets = "".join(
        '<sheet name="{}" sheetId="{}" r:id="rId{}"/>'.format(
            escape(title, {'"': "&quot;"}), index, index
        )
        for index, (title, _rows, _widths) in enumerate(worksheets, 1)
    )
    workbook = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>{}</sheets></workbook>""".format(workbook_sheets)
    worksheet_rels = "".join(
        '<Relationship Id="rId{}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet{}.xml"/>'.format(
            index, index
        )
        for index in range(1, len(worksheets) + 1)
    )
    styles_relation_id = len(worksheets) + 1
    workbook_rels = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">{}<Relationship Id="rId{}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>""".format(worksheet_rels, styles_relation_id)
    styles = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="2"><font><sz val="11"/><name val="Calibri"/></font><font><b/><color rgb="FFFFFFFF"/><sz val="11"/><name val="Calibri"/></font></fonts><fills count="3"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FF4472C4"/><bgColor indexed="64"/></patternFill></fill></fills><borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="2"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/><xf numFmtId="0" fontId="1" fillId="2" borderId="0" xfId="0" applyFont="1" applyFill="1"/></cellXfs></styleSheet>"""
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("[Content_Types].xml", content_types)
        archive.writestr("_rels/.rels", root_rels)
        archive.writestr("xl/workbook.xml", workbook)
        archive.writestr("xl/_rels/workbook.xml.rels", workbook_rels)
        archive.writestr("xl/styles.xml", styles)
        for index, (_title, rows, widths) in enumerate(worksheets, 1):
            archive.writestr(
                "xl/worksheets/sheet{}.xml".format(index),
                worksheet_xml(rows, widths),
            )
    return output


def main():
    args = parse_args()
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    if args.output is None:
        args.output = REPO_ROOT / "micropython-test-report-{}.xlsx".format(timestamp)
    artifact_root = Path(tempfile.gettempdir()) / "codex-artifacts" / "ra8-test-report" / timestamp
    groups = []
    retry_sequence = 0

    for directory_index, test_dir in enumerate(args.test_dirs, 1):
        unittest_candidates = find_unittest_candidates(test_dir)
        target_wiring_candidates = find_target_wiring_candidates(test_dir)
        excluded_candidates = sorted(
            set(unittest_candidates + target_wiring_candidates)
        )
        candidate_results = [
            [test_name, "skip", "requires target wiring"]
            for test_name in target_wiring_candidates
        ]
        dependency_failures = []
        candidate_seconds = 0.0

        for test_name in target_wiring_candidates:
            print("skip  {} (requires target wiring)".format(test_name))

        for candidate_index, test_name in enumerate(unittest_candidates, 1):
            probe_dir = artifact_root / "probe-{:02d}-{:02d}".format(
                directory_index, candidate_index
            )
            probe_log = artifact_root / "probe-{:02d}-{:02d}.log".format(
                directory_index, candidate_index
            )
            print("Checking unittest candidate: {}...".format(test_name))
            return_code, seconds = run_command(
                make_base_command(args, probe_dir) + [test_name],
                probe_dir,
                probe_log,
            )
            candidate_seconds += seconds
            probe_results = load_results(probe_dir, required=False)
            if probe_results is not None:
                if len(probe_results) != 1:
                    raise RuntimeError(
                        "expected one probe result for {}, got {}".format(
                            test_name, len(probe_results)
                        )
                    )
                candidate_results.extend(probe_results)
            elif log_requires_unittest(probe_log):
                dependency_failures.append(
                    [test_name, "fail", "requires unittest"]
                )
                print("skip  {} (requires unittest; report as failed)".format(test_name))
            else:
                raise RuntimeError(
                    "run-tests.py stopped without a result for {} (exit {})".format(
                        test_name, return_code
                    )
                )

        result_dir = artifact_root / "group-{:02d}".format(directory_index)
        command = make_base_command(args, result_dir)
        if excluded_candidates:
            command.extend(["-e", exclude_pattern(excluded_candidates)])
        command.extend(["--test-dirs", test_dir])
        print("Running test directory: {}...".format(test_dir))
        _return_code, bulk_seconds = run_command(
            command,
            result_dir,
            artifact_root / "group-{:02d}.log".format(directory_index),
        )
        initial_results = load_results(result_dir) + candidate_results
        total_seconds = round(bulk_seconds + candidate_seconds, 2)
        failed = [result for result in initial_results if result[1] == "fail"]
        passed = [result for result in initial_results if result[1] == "pass"]
        skipped = [result for result in initial_results if result[1] == "skip"]

        print(
            "Complete {}: {} passed, {} failed, {} missing unittest, {} skipped "
            "({:.2f} seconds).".format(
                test_dir,
                len(passed),
                len(failed),
                len(dependency_failures),
                len(skipped),
                total_seconds,
            )
        )

        retry_results = {}
        if failed:
            print("Retrying {} failed test(s) from {}...".format(len(failed), test_dir))
        for index, result in enumerate(failed, 1):
            retry_sequence += 1
            test_name = normalise_test_name(result[0])
            retry_dir = artifact_root / "retry-{:04d}".format(retry_sequence)
            retry_log = artifact_root / "retry-{:04d}.log".format(retry_sequence)
            print(
                "\rRetry progress: {}/{}".format(index, len(failed)),
                end="",
                flush=True,
            )
            return_code, seconds = run_command(
                make_base_command(args, retry_dir) + [test_name],
                retry_dir,
                retry_log,
            )
            retry_data = load_results(retry_dir)
            if len(retry_data) != 1:
                raise RuntimeError(
                    "expected one retry result for {}, got {}".format(
                        test_name, len(retry_data)
                    )
                )
            retry_results[test_name] = (retry_data[0], return_code, seconds)
        if failed:
            print()

        retry_counts = {
            status: sum(item[0][1] == status for item in retry_results.values())
            for status in ("pass", "fail", "skip", "ignored")
        }
        groups.append(
            {
                "directory": test_dir,
                "seconds": total_seconds,
                "passed": passed,
                "failed": failed,
                "dependency_failures": dependency_failures,
                "skipped": skipped,
                "retry_results": retry_results,
                "retry_counts": retry_counts,
            }
        )

    summary = [
        [
            "测试目录",
            "首次成功",
            "首次失败",
            "缺少 unittest（按失败）",
            "首次跳过",
            "复测成功",
            "复测仍失败",
            "复测跳过",
            "耗时（秒）",
        ]
    ]
    for group in groups:
        summary.append(
            [
                group["directory"],
                len(group["passed"]),
                len(group["failed"]) + len(group["dependency_failures"]),
                len(group["dependency_failures"]),
                len(group["skipped"]),
                group["retry_counts"]["pass"],
                group["retry_counts"]["fail"],
                group["retry_counts"]["skip"],
                group["seconds"],
            ]
        )
    summary.append(
        [
            "合计",
            sum(len(group["passed"]) for group in groups),
            sum(
                len(group["failed"]) + len(group["dependency_failures"])
                for group in groups
            ),
            sum(len(group["dependency_failures"]) for group in groups),
            sum(len(group["skipped"]) for group in groups),
            sum(group["retry_counts"]["pass"] for group in groups),
            sum(group["retry_counts"]["fail"] for group in groups),
            sum(group["retry_counts"]["skip"] for group in groups),
            round(sum(group["seconds"] for group in groups), 2),
        ]
    )

    status_names = {"pass": "复测成功", "fail": "复测仍失败", "skip": "复测时跳过", "ignored": "复测忽略"}
    detail_sheets = []
    used_titles = {"测试汇总"}
    for group in groups:
        passed_names = [normalise_test_name(result[0]) for result in group["passed"]]
        failed_names = [normalise_test_name(result[0]) for result in group["failed"]]
        failed_names.extend(
            "{} [未执行：缺少 unittest，报告按失败]".format(
                normalise_test_name(result[0])
            )
            for result in group["dependency_failures"]
        )
        skipped_names = [normalise_test_name(result[0]) for result in group["skipped"]]
        retried_names = [
            "{} [{}]".format(
                test_name,
                status_names.get(retry[0][1], retry[0][1]),
            )
            for test_name, retry in group["retry_results"].items()
        ]
        rows = list(
            itertools.zip_longest(
                passed_names,
                failed_names,
                skipped_names,
                retried_names,
                fillvalue="",
            )
        )
        if not rows:
            rows = [("", "", "", "")]
        details = [
            [
                "首次成功项目",
                "首次失败项目（含缺少 unittest）",
                "首次跳过项目",
                "失败项目单项复测结果",
            ]
        ]
        details.extend(list(row) for row in rows)
        detail_sheets.append(
            (
                worksheet_title(group["directory"], used_titles),
                details,
            )
        )

    report = write_xlsx(args.output, summary, detail_sheets)
    print("\nExcel report: {}".format(report))
    if args.keep_artifacts:
        print("Logs and raw results: {}".format(artifact_root))
    else:
        import shutil

        shutil.rmtree(artifact_root)
    reported_failures = sum(
        len(group["failed"]) + len(group["dependency_failures"])
        for group in groups
    )
    return 1 if reported_failures else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nTest run cancelled.", file=sys.stderr)
        sys.exit(130)
    except Exception as error:
        print("Error: {}".format(error), file=sys.stderr)
        sys.exit(2)
