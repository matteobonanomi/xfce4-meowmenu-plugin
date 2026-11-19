#!/usr/bin/env python3

from pathlib import Path
import os
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / "dev" / "install.sh"
UNINSTALLER = ROOT / "dev" / "uninstall.sh"


def check_temporary_prefix_is_reconfigured() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        scratch = Path(temporary)
        fake_bin = scratch / "bin"
        build_dir = scratch / "build"
        fake_bin.mkdir()
        build_dir.mkdir()

        prefix_state = scratch / "prefix"
        invocation_log = scratch / "meson-invocations"
        prefix_state.write_text("/tmp/meowmenu-regression\n", encoding="utf-8")

        fake_meson = fake_bin / "meson"
        fake_meson.write_text(
            """#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys

state = Path(os.environ["MEOWMENU_TEST_PREFIX_STATE"])
log = Path(os.environ["MEOWMENU_TEST_INVOCATION_LOG"])
args = sys.argv[1:]
with log.open("a", encoding="utf-8") as output:
    output.write(" ".join(args) + "\\n")

if args[:2] == ["introspect", "--buildoptions"]:
    print(json.dumps([
        {"name": "prefix", "value": state.read_text(encoding="utf-8").strip()},
        {"name": "b_sanitize", "value": ["address"]},
    ]))
elif args and args[0] == "setup":
    prefix = next(arg.split("=", 1)[1] for arg in args if arg.startswith("--prefix="))
    state.write_text(prefix + "\\n", encoding="utf-8")
elif args[:2] == ["introspect", "--installed"]:
    print(json.dumps({"build/libmeowmenu.so": "/usr/local/lib/libmeowmenu.so"}))
else:
    raise SystemExit(f"unexpected fake Meson invocation: {args}")
""",
            encoding="utf-8",
        )
        fake_meson.chmod(0o755)

        environment = os.environ.copy()
        environment.update(
            {
                "HOME": str(scratch / "home"),
                "MEOWMENU_TEST_INVOCATION_LOG": str(invocation_log),
                "MEOWMENU_TEST_PREFIX_STATE": str(prefix_state),
                "PATH": f"{fake_bin}:{environment['PATH']}",
                "XDG_DATA_DIRS": "/usr/local/share:/usr/share",
            }
        )
        result = subprocess.run(
            [str(INSTALLER), str(build_dir)],
            cwd=ROOT,
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )

        assert result.returncode != 0
        assert "uses b_sanitize=address" in result.stdout
        assert prefix_state.read_text(encoding="utf-8").strip() == "/usr/local"
        invocations = invocation_log.read_text(encoding="utf-8")
        assert "setup --reconfigure" in invocations
        assert "--prefix=/usr/local" in invocations
        invocation_lines = invocations.splitlines()
        assert all(not line.startswith("compile ") for line in invocation_lines)
        assert all(not line.startswith("install ") for line in invocation_lines)


def main() -> None:
    source = INSTALLER.read_text(encoding="utf-8")
    uninstall_source = UNINSTALLER.read_text(encoding="utf-8")

    subprocess.run(["bash", "-n", str(INSTALLER)], check=True)
    subprocess.run(["bash", "-n", str(UNINSTALLER)], check=True)
    assert not re.search(r"(^|[;&|\s])rg(\s|$)", source)
    assert "x86_64-linux-gnu" not in source
    assert "meson introspect --installed" in source
    assert "INSTALLED_MODULE" in source
    assert "INSTALLED_PRESETS" in source
    assert "INSTALLED_PAYLOAD" in source
    assert "installed built-in preset is missing" in source
    assert "installed package payload is missing" in source
    assert "--clean-stale-dev" in source
    assert 'STANDARD_PREFIX="/usr/local"' in source
    assert '"--prefix=${STANDARD_PREFIX}"' in source
    assert "XDG_DATA_DIRS does not include /usr/local/share" in source
    assert "Completely remove prior MeowMenu installation and user state" in source
    assert 'uninstall.sh" --no-restart' in source
    assert "PANEL_XDG_DATA_DIRS" not in source
    assert 'XDG_DATA_DIRS="${PANEL_XDG_DATA_DIRS}"' not in source
    assert "MEOWMENU_WAS_CONFIGURED" not in source
    assert "readelf -d" in source
    assert "INSTALLED_SANITIZER_DEPS" in source
    assert "Installed module:" in source
    assert "/usr/local" in source

    assert "is_protected_system_path" in uninstall_source
    assert "XDG_DATA_HOME" in uninstall_source
    assert "XDG_CONFIG_HOME" in uninstall_source
    assert "XDG_CACHE_HOME" in uninstall_source
    assert "USER_CACHE_DIR" in uninstall_source
    assert "Known temporary MeowMenu prefixes are searched as well." in uninstall_source
    assert "remove_dev_payload_under_root" in uninstall_source
    assert "--no-restart" in uninstall_source
    assert "Stop Xfce panel before complete cleanup" in uninstall_source
    assert "Remove every MeowMenu panel slot and its Xfconf settings" in uninstall_source
    assert "plugin-ids" in uninstall_source
    assert "--force-array" in uninstall_source
    assert "--reset --recursive" in uninstall_source
    assert "xfce4-meowmenu-plugin.appdata.xml" in uninstall_source
    assert "a package-managed MeowMenu installation is present" in uninstall_source
    assert "xfce4-meowmenu-plugin package" in uninstall_source
    assert "restore_panel_after_failure" in source
    assert "restore_panel_after_failure" in uninstall_source

    compile_payload = source.index('meson compile -C "${BUILD_DIR}"')
    clean_uninstall = source.index('uninstall.sh" --no-restart')
    install_payload = source.index("meson install --no-rebuild")
    assert compile_payload < clean_uninstall < install_payload

    stop_panel = uninstall_source.index("Stop Xfce panel before complete cleanup")
    clean_payload = uninstall_source.index('KNOWN_ROOTS=("/usr/local")')
    clean_settings = uninstall_source.index(
        "Remove every MeowMenu panel slot and its Xfconf settings"
    )
    restart_panel = uninstall_source.index("Restart Xfce panel without MeowMenu slots")
    assert stop_panel < clean_settings < clean_payload < restart_panel

    check_temporary_prefix_is_reconfigured()


if __name__ == "__main__":
    main()
