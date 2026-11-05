#!/usr/bin/env python3
"""Verify the Geometry Dash version embedded in every built .geode package."""
import json
import pathlib
import sys
import zipfile

if len(sys.argv) != 4:
    raise SystemExit("usage: verify_geode_package.py <package-dir> <platform-key> <expected-version>")

package_dir = pathlib.Path(sys.argv[1])
platform_key = sys.argv[2]
expected = sys.argv[3]
source_path = pathlib.Path("mod.json")
source = json.loads(source_path.read_text(encoding="utf-8"))
source_version = source.get("gd", {}).get(platform_key)

if source_version != expected:
    raise SystemExit(
        f"mod.json {platform_key} version is {source_version!r}, expected {expected!r}"
    )

packages = sorted(package_dir.glob("*.geode"))
if not packages:
    raise SystemExit(f"No .geode package found in {package_dir}")

for package in packages:
    try:
        with zipfile.ZipFile(package) as archive:
            metadata = json.loads(archive.read("mod.json").decode("utf-8"))
    except Exception as exc:
        raise SystemExit(f"Cannot read {package}: {exc}") from exc

    actual = metadata.get("gd", {}).get(platform_key)
    if actual != expected:
        raise SystemExit(
            f"VERSION MISMATCH: {package.name} declares GD {actual!r} "
            f"for {platform_key}; expected {expected!r}. Package rejected."
        )

    if metadata.get("id") != source.get("id"):
        raise SystemExit(
            f"MOD ID MISMATCH: {package.name} contains {metadata.get('id')!r}, "
            f"expected {source.get('id')!r}"
        )

    print(f"Verified {package.name}: Geometry Dash {actual} ({platform_key})")
