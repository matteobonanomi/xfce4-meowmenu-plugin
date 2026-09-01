#!/usr/bin/env python3

from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / "dev" / "install.sh"


def main() -> None:
    source = INSTALLER.read_text(encoding="utf-8")

    subprocess.run(["bash", "-n", str(INSTALLER)], check=True)
    assert not re.search(r"(^|[;&|\s])rg(\s|$)", source)
    assert "x86_64-linux-gnu" not in source
    assert "meson introspect --installed" in source
    assert "INSTALLED_MODULE" in source
    assert "INSTALLED_PRESETS" in source
    assert "INSTALLED_PAYLOAD" in source
    assert "installed built-in preset is missing" in source
    assert "installed package payload is missing" in source
    assert "pgrep -f 'wrapper-2\\.0 .*libmeowmenu\\.so'" in source

    remove_data = source.index('rm -rf -- "${USER_MEOWMENU_DATA}"')
    install_payload = source.index("meson install --no-rebuild")
    assert remove_data < install_payload


if __name__ == "__main__":
    main()
