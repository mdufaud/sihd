#!/usr/bin/env python3
"""
Usage: merge_cppcheck_xml.py <output.xml> <report1.xml> [report2.xml ...]

Merges multiple cppcheck XML v2 reports into a single file,
keeping only errors with severity >= error (i.e. severity="error").
"""

import sys
import xml.etree.ElementTree as ET

ERROR_SEVERITIES = {"error"}


def main() -> None:
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <output.xml> <report1.xml> [report2.xml ...]", file=sys.stderr)
        sys.exit(1)

    output_path = sys.argv[1]
    input_files = sys.argv[2:]

    errors: list[ET.Element] = []
    cppcheck_version = "unknown"

    for path in input_files:
        try:
            root = ET.parse(path).getroot()
        except ET.ParseError as exc:
            print(f"[warn] could not parse {path}: {exc}", file=sys.stderr)
            continue

        for el in root.iter("cppcheck"):
            cppcheck_version = el.get("version", cppcheck_version)

        for error in root.iter("error"):
            if error.get("severity") in ERROR_SEVERITIES:
                errors.append(error)

    results = ET.Element("results", {"version": "2"})
    ET.SubElement(results, "cppcheck", {"version": cppcheck_version})
    errors_el = ET.SubElement(results, "errors")
    for error in errors:
        errors_el.append(error)

    ET.indent(results)
    tree = ET.ElementTree(results)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write(ET.tostring(results, encoding="unicode"))
        f.write("\n")

    print(f"[info] merged {len(errors)} error(s) from {len(input_files)} report(s) into {output_path}")


if __name__ == "__main__":
    main()
