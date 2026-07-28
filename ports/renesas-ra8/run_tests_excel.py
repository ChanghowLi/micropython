#!/usr/bin/env python3
"""Run MicroPython tests on a serial target and create an Excel report."""

import argparse
import datetime
import itertools
import json
import os
from pathlib import Path
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
        default=REPO_ROOT / "micropython-test-report.xlsx",
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


def load_results(result_dir):
    results_file = result_dir / "_results.json"
    if not results_file.is_file():
        raise RuntimeError("run-tests.py did not create {}".format(results_file))
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


def write_xlsx(output, summary_rows, detail_rows):
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    content_types = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/worksheets/sheet2.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>"""
    root_rels = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>"""
    workbook = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="测试汇总" sheetId="1" r:id="rId1"/><sheet name="测试明细" sheetId="2" r:id="rId2"/></sheets></workbook>"""
    workbook_rels = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/><Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>"""
    styles = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="2"><font><sz val="11"/><name val="Calibri"/></font><font><b/><color rgb="FFFFFFFF"/><sz val="11"/><name val="Calibri"/></font></fonts><fills count="3"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FF4472C4"/><bgColor indexed="64"/></patternFill></fill></fills><borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="2"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/><xf numFmtId="0" fontId="1" fillId="2" borderId="0" xfId="0" applyFont="1" applyFill="1"/></cellXfs></styleSheet>"""
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("[Content_Types].xml", content_types)
        archive.writestr("_rels/.rels", root_rels)
        archive.writestr("xl/workbook.xml", workbook)
        archive.writestr("xl/_rels/workbook.xml.rels", workbook_rels)
        archive.writestr("xl/styles.xml", styles)
        archive.writestr("xl/worksheets/sheet1.xml", worksheet_xml(summary_rows, [58, 50]))
        archive.writestr(
            "xl/worksheets/sheet2.xml",
            worksheet_xml(detail_rows, [58, 58, 58, 72]),
        )
    return output


def main():
    args = parse_args()
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    artifact_root = Path(tempfile.gettempdir()) / "codex-artifacts" / "ra8-test-report" / timestamp
    bulk_dir = artifact_root / "bulk"
    bulk_command = make_base_command(args, bulk_dir) + ["--test-dirs", *args.test_dirs]

    print("Running the complete test selection...")
    bulk_return_code, bulk_seconds = run_command(bulk_command, bulk_dir, artifact_root / "bulk.log")
    initial_results = load_results(bulk_dir)

    failed = [result for result in initial_results if result[1] == "fail"]
    initial_counts = {
        status: sum(result[1] == status for result in initial_results)
        for status in ("pass", "fail", "skip", "ignored")
    }
    print(
        "Complete: {} passed, {} failed, {} skipped ({:.2f} seconds).".format(
            initial_counts["pass"],
            initial_counts["fail"],
            initial_counts["skip"],
            bulk_seconds,
        )
    )
    retry_results = {}
    print("\nRetrying {} failed test(s) individually...".format(len(failed)))
    for index, result in enumerate(failed, 1):
        test_name = normalise_test_name(result[0])
        retry_dir = artifact_root / "retry-{:04d}".format(index)
        command = make_base_command(args, retry_dir) + [test_name]
        print("\rRetry progress: {}/{}".format(index, len(failed)), end="", flush=True)
        return_code, seconds = run_command(command, retry_dir, artifact_root / "retry-{:04d}.log".format(index))
        retry_data = load_results(retry_dir)
        if len(retry_data) != 1:
            raise RuntimeError("expected one retry result for {}, got {}".format(test_name, len(retry_data)))
        retry_results[test_name] = (retry_data[0], return_code, seconds)
    if failed:
        print()

    counts = initial_counts
    retry_counts = {status: sum(item[0][1] == status for item in retry_results.values()) for status in ("pass", "fail", "skip", "ignored")}
    summary = [
        ["项目", "结果或说明"],
        ["报告生成时间", datetime.datetime.now().isoformat(timespec="seconds")],
        ["测试设备串口", args.device],
        ["串口波特率", args.baudrate],
        ["本次测试目录", ", ".join(args.test_dirs)],
        ["首次批量测试成功数量", counts["pass"]],
        ["首次批量测试失败数量", counts["fail"]],
        ["首次批量测试跳过数量（未实际执行）", counts["skip"]],
        ["初次失败、单项复测后成功的数量", retry_counts["pass"]],
        ["初次失败、单项复测后仍失败的数量", retry_counts["fail"]],
        ["初次失败、单项复测时变为跳过的数量", retry_counts["skip"]],
    ]
    passed_names = [normalise_test_name(result[0]) for result in initial_results if result[1] == "pass"]
    failed_names = [normalise_test_name(result[0]) for result in initial_results if result[1] == "fail"]
    skipped_names = [normalise_test_name(result[0]) for result in initial_results if result[1] == "skip"]
    status_names = {"pass": "复测成功", "fail": "复测仍失败", "skip": "复测时跳过", "ignored": "复测忽略"}
    retried_names = [
        "{} [{}]".format(test_name, status_names.get(retry[0][1], retry[0][1]))
        for test_name, retry in retry_results.items()
    ]
    details = [["首次批量测试：成功项目", "首次批量测试：失败项目", "首次批量测试：跳过项目", "初次失败项目：单项复测结果"]]
    details.extend(
        list(row)
        for row in itertools.zip_longest(
            passed_names, failed_names, skipped_names, retried_names, fillvalue=""
        )
    )
    report = write_xlsx(args.output, summary, details)
    print("\nExcel report: {}".format(report))
    if args.keep_artifacts:
        print("Logs and raw results: {}".format(artifact_root))
    else:
        import shutil

        shutil.rmtree(artifact_root)
    return 1 if counts["fail"] else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nTest run cancelled.", file=sys.stderr)
        sys.exit(130)
    except Exception as error:
        print("Error: {}".format(error), file=sys.stderr)
        sys.exit(2)
