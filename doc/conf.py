# Copyright (c) 2026 Muhammad Waleed Badar
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / "_extensions"))

project = "ZSBand"
author = "Muhammad Waleed Badar"
copyright = "2026, Muhammad Waleed Badar"

extensions = [
    "sphinx.ext.intersphinx",
    "zephyr_compat",
]

root_doc = "index"
exclude_patterns = ["_build", "binary", "datasheets"]

intersphinx_mapping = {
    "zephyr": ("https://docs.zephyrproject.org/latest", None),
}

html_theme = "sphinx_rtd_theme"
html_title = "ZSBand documentation"
html_show_sourcelink = False
html_context = {
    "display_github": True,
    "github_user": "walidbadar",
    "github_repo": "ZSband",
    "github_version": "main",
    "conf_py_path": "/doc/",
}
