#!/usr/bin/env python3
"""Run the maintainer-facing localization inventory and catalog audit."""

import ast
import csv
import importlib.util
import os
import re
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STATUS_SPEC = importlib.util.spec_from_file_location(
    "translation_status", ROOT / "build-aux" / "translation-status.py"
)
STATUS = importlib.util.module_from_spec(STATUS_SPEC)
STATUS_SPEC.loader.exec_module(STATUS)


# This is the known message-bearing source inventory. It is intentionally
# independent of the mutable POTFILES file so omissions remain detectable.
KNOWN_MESSAGE_SOURCES = (
    "panel-plugin/launcher/applications-page.cpp",
    "panel-plugin/launcher/category.cpp",
    "panel-plugin/ui/command-edit.cpp",
    "panel-plugin/launcher/element.cpp",
    "panel-plugin/launcher/favorites-page.cpp",
    "panel-plugin/places/favourites-section.cpp",
    "panel-plugin/places/history-section.cpp",
    "panel-plugin/places/home-section.cpp",
    "panel-plugin/ui/icon-size.cpp",
    "panel-plugin/launcher/launcher.cpp",
    "panel-plugin/launcher/page.cpp",
    "panel-plugin/places/places-item.cpp",
    "panel-plugin/places/places-page.cpp",
    "panel-plugin/core/plugin.cpp",
    "panel-plugin/launcher/recent-page.cpp",
    "panel-plugin/search/run-action.cpp",
    "panel-plugin/search/search-action.cpp",
    "panel-plugin/search/search-page.cpp",
    "panel-plugin/search/calculator-result.cpp",
    "panel-plugin/settings-dialog.cpp",
    "panel-plugin/ui/properties/general.cpp",
    "panel-plugin/ui/properties/extras.cpp",
    "panel-plugin/ui/properties/places.cpp",
    "panel-plugin/ui/properties/results-view.cpp",
    "panel-plugin/ui/properties/search-bar.cpp",
    "panel-plugin/ui/properties/search-bar-aliases.cpp",
    "panel-plugin/ui/properties/search-bar-actions.cpp",
    "panel-plugin/ui/properties/sidebar.cpp",
    "panel-plugin/ui/properties/user-session.cpp",
    "panel-plugin/settings.cpp",
    "panel-plugin/settings-defaults.cpp",
    "panel-plugin/calculator/calculator-engine.cpp",
    "panel-plugin/core/window.cpp",
    "panel-plugin/core/window-pages.cpp",
    "panel-plugin/core/xfce4-popup-meowmenu.cpp",
    "panel-plugin/presets/preset-builtins.cpp",
    "panel-plugin/presets/preset-io.cpp",
    "panel-plugin/meowmenu.desktop.in",
    "data/metainfo/io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml",
)

def _run(command, cwd=None, check=True):
    environment = dict(os.environ)
    environment["LC_ALL"] = "C"
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment,
    )
    if check and result.returncode:
        detail = (result.stderr or result.stdout).strip()
        raise RuntimeError(f"{' '.join(map(str, command))}: {detail}")
    return result


def _field_value(lines, token):
    values = []
    for index, line in enumerate(lines):
        if line.startswith(token + " "):
            values.append(ast.literal_eval(line[len(token) + 1:].strip()))
            cursor = index + 1
            while cursor < len(lines) and lines[cursor].startswith('"'):
                values.append(ast.literal_eval(lines[cursor].strip()))
                cursor += 1
            return "".join(values)
    return None


def po_identities(path):
    """Return gettext identities, excluding the header and obsolete blocks."""
    identities = set()
    content = Path(path).read_text(encoding="utf-8")
    for block_text in content.split("\n\n"):
        lines = block_text.splitlines()
        msgid = _field_value(lines, "msgid")
        if not msgid:
            continue
        context = _field_value(lines, "msgctxt")
        plural = _field_value(lines, "msgid_plural")
        identities.add((context, msgid, plural))
    return identities


def po_blocks(path):
    """Yield active PO blocks for fuzzy and previous-source checks."""
    content = Path(path).read_text(encoding="utf-8")
    for block_text in content.split("\n\n"):
        if "#~ msgid " in block_text:
            continue
        yield block_text.splitlines()


def potfiles(root):
    """Return nonempty POTFILES entries and diagnostics for missing files."""
    path = Path(root) / "po/POTFILES"
    entries = []
    errors = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        entries.append(line)
        if not (Path(root) / line).is_file():
            errors.append(f"po/POTFILES lists missing source: {line}")
    return entries, errors


def source_membership(root):
    """Check mutable POTFILES membership against the known source inventory."""
    entries, errors = potfiles(root)
    expected = set(KNOWN_MESSAGE_SOURCES)
    listed = set(entries)
    errors.extend(
        f"po/POTFILES omits known message-bearing source: {source}"
        for source in sorted(expected - listed)
    )
    errors.extend(
        f"po/POTFILES contains an unapproved or message-free source: {source}"
        for source in sorted(listed - expected)
    )
    return errors


def fresh_pot_identities(root):
    """Extract a temporary POT with the same gettext options as the build."""
    root = Path(root)
    with tempfile.NamedTemporaryFile(prefix="meowmenu-audit-", suffix=".pot") as temp:
        command = [
            "xgettext", "--from-code=UTF-8", "--add-comments",
            "--keyword=_", "--keyword=N_", "--keyword=C_:1c,2",
            "--keyword=NC_:1c,2", "--keyword=g_dcgettext:2",
            "--keyword=g_dngettext:2,3", "--keyword=g_dpgettext2:2c,3",
            "--flag=N_:1:pass-c-format", "--flag=C_:2:pass-c-format",
            "--flag=NC_:2:pass-c-format", "--flag=g_dngettext:2:pass-c-format",
            "--flag=g_strdup_printf:1:c-format", "--flag=g_string_printf:2:c-format",
            "--flag=g_string_append_printf:2:c-format", "--flag=g_error_new:3:c-format",
            "--flag=g_set_error:4:c-format", "--flag=g_markup_printf_escaped:1:c-format",
            "--flag=g_log:3:c-format", "--flag=g_print:1:c-format",
            "--flag=g_printerr:1:c-format", "--flag=g_printf:1:c-format",
            "--flag=g_fprintf:2:c-format", "--flag=g_sprintf:2:c-format",
            "--flag=g_snprintf:3:c-format", "--directory", str(root),
            "--files-from", str(root / "po/POTFILES"), "--output", temp.name,
        ]
        result = _run(command, cwd=root, check=False)
        if result.returncode:
            raise RuntimeError(result.stderr.strip())
        return po_identities(temp.name)


def inventory_diagnostics(root):
    """Detect semantic drift between fresh extraction and the committed POT."""
    root = Path(root)
    committed = po_identities(root / "po/xfce4-meowmenu-plugin.pot")
    fresh = fresh_pot_identities(root)
    errors = []
    for identity in sorted(fresh - committed, key=str):
        errors.append(f"POT is missing freshly extracted identity: {identity[1]}")
    for identity in sorted(committed - fresh, key=str):
        errors.append(f"POT contains stale identity: {identity[1]}")
    return errors


def catalog_diagnostics(root):
    """Check catalog syntax, debris, retired identities, and unresolved fuzzy entries."""
    root = Path(root)
    pot = root / "po/xfce4-meowmenu-plugin.pot"
    current = po_identities(pot)
    errors = []
    for locale in STATUS.EXPECTED_LOCALES:
        path = root / "po" / f"{locale}.po"
        if not path.is_file():
            errors.append(f"missing catalog: {path.relative_to(root)}")
            continue
        try:
            STATUS.catalog_statistics(path, pot)
        except STATUS.CatalogError as error:
            errors.append(str(error))
        retired = po_identities(path) - current
        errors.extend(
            f"{locale}: retired active identity: {identity[1]}"
            for identity in sorted(retired, key=str)
        )
        content = path.read_text(encoding="utf-8")
        if re.search(r"^#~ msgid ", content, re.MULTILINE):
            errors.append(f"{locale}: obsolete catalog entries remain")
        if re.search(r"^#\| msgid ", content, re.MULTILINE):
            errors.append(f"{locale}: previous-source blocks remain")
        for block in po_blocks(path):
            if any(line.startswith("#,") and "fuzzy" in line for line in block):
                msgid = _field_value(block, "msgid")
                errors.append(f"{locale}: unresolved fuzzy identity: {msgid}")
    return errors


def ledger_diagnostics(root):
    """Ensure focused review rows carry a reproducible final disposition."""
    path = (
        Path(root) / (".spec" + "ify") / "specs" / "065-localization-update"
        / "translation-review.csv"
    )
    if not path.is_file():
        return [f"missing review ledger: {path}"]
    required = {
        "locale", "context", "msgid", "plural", "baseline_state",
        "candidate_origin", "technical_check", "context_check",
        "terminology_check", "self_evaluation", "final_state", "note",
    }
    errors = []
    with path.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None or set(reader.fieldnames) != required:
            return ["review ledger has an unexpected header"]
        for number, row in enumerate(reader, start=2):
            if not row["locale"] or not row["msgid"]:
                errors.append(f"review ledger row {number} lacks an identity")
            if row["final_state"] not in {"translated", "untranslated", "removed"}:
                errors.append(f"review ledger row {number} lacks a final disposition")
    return errors


def status_diagnostics(root):
    """Detect stale or manually altered generated status documentation."""
    root = Path(root)
    try:
        expected = STATUS.expected_document(root)
    except (OSError, STATUS.CatalogError) as error:
        return [f"status generation failed: {error}"]
    path = root / "docs/translations.md"
    if not path.is_file() or path.read_text(encoding="utf-8") != expected:
        return ["docs/translations.md is stale or manually altered"]
    return []


def all_diagnostics(root):
    """Run all automatic checks; the visible-surface review remains manual."""
    errors = []
    errors.extend(source_membership(root))
    errors.extend(inventory_diagnostics(root))
    errors.extend(STATUS.validate_locale_set(root))
    errors.extend(catalog_diagnostics(root))
    errors.extend(ledger_diagnostics(root))
    errors.extend(status_diagnostics(root))
    return errors


def _fixture_catalog(language="xx", translated=True):
    value = "known" if translated else ""
    return (
        'msgid ""\nmsgstr ""\n'
        f'"Language: {language}\\n"\n'
        '"Plural-Forms: nplurals=2; plural=(n != 1);\\n"\n\n'
        'msgid "known"\n'
        f'msgstr "{value}"\n'
    )


def run_self_test():
    """Exercise deliberate mutations in an isolated temporary repository."""
    with tempfile.TemporaryDirectory(prefix="meowmenu-localization-audit-") as temporary:
        root = Path(temporary)
        (root / "po").mkdir()
        (root / "data/metainfo").mkdir(parents=True)
        (root / "docs").mkdir()
        (
            root / (".spec" + "ify") / "specs" / "065-localization-update"
        ).mkdir(parents=True)
        files = list(KNOWN_MESSAGE_SOURCES)
        for relative in files:
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('', encoding='utf-8')
        first_source = root / KNOWN_MESSAGE_SOURCES[0]
        first_source.write_text('const char* label = _("known");\n', encoding='utf-8')
        (root / "po/POTFILES").write_text("\n".join(files) + "\n", encoding="utf-8")
        (root / "po/xfce4-meowmenu-plugin.pot").write_text(
            'msgid ""\nmsgstr ""\n"Language: C\\n"\n\nmsgid "known"\nmsgstr ""\n',
            encoding='utf-8',
        )
        (root / "po/LINGUAS").write_text("\n".join(STATUS.EXPECTED_LOCALES) + "\n", encoding='utf-8')
        for locale in STATUS.EXPECTED_LOCALES:
            (root / "po" / f"{locale}.po").write_text(
                _fixture_catalog(locale), encoding='utf-8'
            )
        records = STATUS.inventory(root)
        (root / "docs/translations.md").write_text(
            STATUS.update_document("# Status\n", records), encoding='utf-8'
        )
        (
            root / (".spec" + "ify") / "specs" / "065-localization-update"
            / "translation-review.csv"
        ).write_text(
            "locale,context,msgid,plural,baseline_state,candidate_origin,technical_check,context_check,terminology_check,self_evaluation,final_state,note\n",
            encoding='utf-8',
        )

        potfiles = (root / "po/POTFILES").read_text(encoding='utf-8')
        (root / "po/POTFILES").write_text(
            potfiles.replace(files[0] + "\n", "", 1), encoding='utf-8'
        )
        assert source_membership(root)
        (root / "po/POTFILES").write_text(potfiles, encoding='utf-8')

        (root / "po/xfce4-meowmenu-plugin.pot").write_text(
            'msgid ""\nmsgstr ""\n"Language: C\\n"\n\nmsgid "different"\nmsgstr ""\n',
            encoding='utf-8',
        )
        assert inventory_diagnostics(root)
        (root / "po/xfce4-meowmenu-plugin.pot").write_text(
            'msgid ""\nmsgstr ""\n"Language: C\\n"\n\nmsgid "known"\nmsgstr ""\n',
            encoding='utf-8',
        )

        linguas = (root / "po/LINGUAS").read_text(encoding='utf-8')
        (root / "po/LINGUAS").write_text(linguas.replace("am\n", "", 1), encoding='utf-8')
        assert STATUS.validate_locale_set(root)
        (root / "po/LINGUAS").write_text(linguas, encoding='utf-8')

        catalog = root / "po/am.po"
        catalog.write_text(catalog.read_text(encoding='utf-8') + '\n#~ msgid "retired"\n#~ msgstr "old"\n', encoding='utf-8')
        assert catalog_diagnostics(root)
        catalog.write_text(_fixture_catalog("am"), encoding='utf-8')

        docs = root / "docs/translations.md"
        docs.write_text(docs.read_text(encoding='utf-8').replace("| `am` | Pass | 1 |", "| `am` | Pass | 2 |", 1), encoding='utf-8')
        assert status_diagnostics(root)
        docs.write_text(STATUS.update_document("# Status\n", STATUS.inventory(root)), encoding='utf-8')

        invalid = root / "po/invalid.po"
        invalid.write_text(
            'msgid ""\nmsgstr ""\n"Language: xx\\n"\n\n'
            '#, c-format\nmsgid "Value: %s"\nmsgstr "Value"\n',
            encoding='utf-8',
        )
        try:
            STATUS.catalog_statistics(invalid, root / "po/xfce4-meowmenu-plugin.pot")
        except STATUS.CatalogError:
            pass
        else:
            raise AssertionError("invalid placeholder was not detected")
    return True


def main():
    parser = __import__('argparse').ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        run_self_test()
        print('localization audit self-test passed')
        return 0
    errors = all_diagnostics(args.root)
    if errors:
        raise SystemExit("\n".join(errors))
    print('localization audit passed; visible-surface review remains a maintainer check')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
