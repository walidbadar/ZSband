# Copyright (c) 2026 Muhammad Waleed Badar
# SPDX-License-Identifier: Apache-2.0

"""Minimal stand-in for the Zephyr documentation Sphinx domain.

Board documentation in this repository follows the upstream Zephyr format so
it can be submitted upstream unchanged. Upstream uses the ``zephyr`` domain
(``zephyr:board`` and friends) which relies on Zephyr's board catalog. This
extension implements just enough of it to build the docs standalone:

- ``.. zephyr:board:: <name>`` turns the page into a section titled with the
  board's ``full_name`` from ``board.yml`` and adds a short summary table.
- ``.. zephyr:board-supported-hw::`` lists the ``supported`` features from the
  board's twister YAML file.
- ``.. zephyr:board-supported-runners::`` lists the runners included in
  ``board.cmake``.
- ``:zephyr:code-sample:`` / ``:zephyr:board:`` references are resolved
  against the upstream Zephyr docs through intersphinx.
"""

import re
from pathlib import Path

import yaml
from docutils import nodes
from sphinx.domains import Domain, ObjType
from sphinx.roles import XRefRole
from sphinx.transforms import SphinxTransform
from sphinx.util.docutils import SphinxDirective


def _board_dir(env, docname):
    """Return the board directory (the one holding board.yml) for a doc."""
    path = Path(env.doc2path(docname)).resolve().parent
    for candidate in (path, *path.parents):
        if (candidate / "board.yml").is_file():
            return candidate
    return None


def _table(rows, header):
    table = nodes.table()
    tgroup = nodes.tgroup(cols=len(header))
    table += tgroup
    for _ in header:
        tgroup += nodes.colspec(colwidth=1)
    thead = nodes.thead()
    tgroup += thead
    tbody = nodes.tbody()
    tgroup += tbody

    def add_row(parent, cells):
        row = nodes.row()
        for cell in cells:
            entry = nodes.entry()
            entry += cell if isinstance(cell, nodes.Node) else nodes.paragraph(text=str(cell))
            row += entry
        parent += row

    add_row(thead, header)
    for r in rows:
        add_row(tbody, r)
    return table


class board_node(nodes.Element):
    """Placeholder replaced by a titled section in BoardTransform."""


class BoardDirective(SphinxDirective):
    required_arguments = 1

    def run(self):
        name = self.arguments[0]
        board_dir = _board_dir(self.env, self.env.docname)
        if board_dir is None:
            raise self.error(f"board.yml not found for board {name}")

        self.env.note_dependency(str(board_dir / "board.yml"))
        meta = yaml.safe_load((board_dir / "board.yml").read_text())["board"]

        node = board_node()
        node["name"] = name
        node["full_name"] = meta.get("full_name", name)
        node["vendor"] = meta.get("vendor", "")
        node["socs"] = [s["name"] for s in meta.get("socs", [])]
        return [node]


class BoardTransform(SphinxTransform):
    """Wrap everything after a ``zephyr:board`` node into a titled section."""

    default_priority = 100

    def apply(self):
        for node in list(self.document.findall(board_node)):
            parent = node.parent
            idx = parent.index(node)
            siblings = parent.children[idx + 1 :]

            section = nodes.section(ids=[nodes.make_id(node["name"])], names=[node["name"]])
            section += nodes.title(text=node["full_name"])
            section += _table(
                [
                    ["Board", nodes.paragraph("", "", nodes.literal(text=node["name"]))],
                    ["Vendor", node["vendor"]],
                    ["SoC", ", ".join(node["socs"])],
                ],
                ["", ""],
            )
            for sibling in siblings:
                parent.remove(sibling)
                section += sibling
            node.replace_self(section)


class SupportedHwDirective(SphinxDirective):
    def run(self):
        board_dir = _board_dir(self.env, self.env.docname)
        rows = []
        for twister in sorted(board_dir.glob("*.yaml")):
            data = yaml.safe_load(twister.read_text())
            if not isinstance(data, dict) or "identifier" not in data:
                continue
            self.env.note_dependency(str(twister))
            for feature in data.get("supported", []):
                rows.append([feature, data["identifier"]])

        if not rows:
            return [nodes.paragraph(text="No supported features listed.")]

        intro = nodes.paragraph(
            text="The following features are supported (from the board's twister YAML file):"
        )
        return [intro, _table(rows, ["Feature", "Target"])]


class SupportedRunnersDirective(SphinxDirective):
    _include = re.compile(r"include\(\$\{ZEPHYR_BASE\}/boards/common/(\S+)\.board\.cmake\)")

    def run(self):
        board_dir = _board_dir(self.env, self.env.docname)
        cmake = board_dir / "board.cmake"
        if not cmake.is_file():
            return []
        self.env.note_dependency(str(cmake))

        runners = self._include.findall(cmake.read_text())
        rows = [[r, "yes" if i == 0 else ""] for i, r in enumerate(runners)]
        intro = nodes.paragraph(text="The following west runners are supported:")
        return [intro, _table(rows, ["Runner", "Default"])]


class ZephyrDomain(Domain):
    name = "zephyr"
    label = "Zephyr"
    object_types = {
        "code-sample": ObjType("code sample", "code-sample"),
        "board": ObjType("board", "board"),
    }
    roles = {
        "code-sample": XRefRole(),
        "board": XRefRole(),
    }
    directives = {
        "board": BoardDirective,
        "board-supported-hw": SupportedHwDirective,
        "board-supported-runners": SupportedRunnersDirective,
    }
    initial_data = {}

    def resolve_xref(self, env, fromdocname, builder, typ, target, node, contnode):
        # Everything is resolved by intersphinx against the upstream docs.
        return None

    def merge_domaindata(self, docnames, otherdata):
        pass


def setup(app):
    app.add_node(board_node)
    app.add_domain(ZephyrDomain)
    app.add_transform(BoardTransform)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
