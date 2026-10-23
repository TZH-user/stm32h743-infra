#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AutoBench 命令行入口。"""

from __future__ import annotations

import sys

from autobench_core.runner import run_cli


if __name__ == "__main__":
    sys.exit(run_cli())

