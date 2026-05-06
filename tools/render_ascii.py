#!/usr/bin/env python3
import argparse
import os
import sys
import pathlib


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--font-file", required=True)
    p.add_argument("--text", required=True)
    args = p.parse_args()

    root = os.path.dirname(os.path.dirname(__file__))
    sys.path.insert(0, os.path.join(root, "tools"))
    from pyfiglet import Figlet  # type: ignore

    font_path = pathlib.Path(args.font_file)
    font_dir = str(font_path.parent)
    font_name = font_path.stem
    old_cwd = os.getcwd()
    try:
        os.chdir(font_dir)
        fig = Figlet(font=font_name)
        out = fig.renderText(args.text)
    finally:
        os.chdir(old_cwd)
    sys.stdout.write(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
