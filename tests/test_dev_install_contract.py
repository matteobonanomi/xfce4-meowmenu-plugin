#!/usr/bin/env python3

from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / "dev" / "install.sh"
UNINSTALLER = ROOT / "dev" / "uninstall.sh"


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
    assert "pgrep -f 'wrapper-2\\.0 .*libmeowmenu\\.so'" in source
    assert "--clean-stale-dev" in source
    assert "Potential conflicting MeowMenu development modules found" in source
    assert "readelf -d" in source
    assert "INSTALLED_SANITIZER_DEPS" in source
    assert "Installed module:" in source
    assert "/usr/local" in source
    assert source.index("collect_known_modules") < source.index("meson compile -C")

    assert "is_protected_system_path" in uninstall_source
    assert "XDG_DATA_HOME" in uninstall_source
    assert "XDG_CACHE_HOME" in uninstall_source
    assert "USER_CACHE_DIR" in uninstall_source
    assert "Other custom prefixes are not searched" in uninstall_source

    remove_data = source.index('rm -rf -- "${USER_MEOWMENU_DATA}"')
    install_payload = source.index("meson install --no-rebuild")
    assert remove_data < install_payload


if __name__ == "__main__":
    main()
