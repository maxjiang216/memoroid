#!/usr/bin/env python3
"""
Import assets/tmp/{slug}_{anything}.jpg|.png into assets/characters/{slug}/ as BMP,
using sequential names: {slug}_001.bmp, {slug}_002.bmp, …

All character BMPs are normalized to that scheme (see `normalize` subcommand).

Existing art is uniformly 480×648 (aspect 20∶27). Wrong aspect ratios are skipped.

Requires ImageMagick (`magick` or `convert`). Optional: Pillow / ffprobe for dimensions.

Usage:
  ./scripts/import_tmp_to_characters.py normalize [--dry-run]
  ./scripts/import_tmp_to_characters.py import [--dry-run] [--skip-normalize]
  ./scripts/import_tmp_to_characters.py manifest   # regenerate generated/wasm_character_manifest.cpp

`import` is the default subcommand (e.g. `./script.py --dry-run` runs import).
BMPs are named {slug}_001.bmp, {slug}_002.bmp, … (width padded when needed).
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REF_W = 480
REF_H = 648
REF_RATIO = REF_W / REF_H

STEM_RE = re.compile(
    r"^([a-z0-9]+)_(.+)\.(png|jpg|jpeg|PNG|JPG|JPEG)$", re.IGNORECASE
)

# Canonical numbered file: kanade_001.bmp (slug lowercase, 3+ digit index)
NUMBERED_RE = re.compile(r"^([a-z0-9]+)_(\d+)\.bmp$", re.IGNORECASE)


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def find_magick() -> list[str] | None:
    for cmd in (["magick"], ["convert"]):
        try:
            subprocess.run(
                cmd + ["-version"],
                capture_output=True,
                check=True,
                timeout=10,
            )
            return cmd
        except (FileNotFoundError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
            continue
    return None


def dims_via_identify(path: Path) -> tuple[int, int] | None:
    try:
        out = subprocess.run(
            ["identify", "-format", "%w %h", str(path)],
            capture_output=True,
            text=True,
            check=True,
            timeout=30,
        )
        parts = out.stdout.strip().split()
        if len(parts) == 2:
            return int(parts[0]), int(parts[1])
    except (FileNotFoundError, subprocess.CalledProcessError, ValueError, subprocess.TimeoutExpired):
        pass
    return None


def dims_via_pillow(path: Path) -> tuple[int, int] | None:
    try:
        from PIL import Image  # type: ignore

        with Image.open(path) as im:
            return im.size
    except Exception:
        return None


def dims_via_ffprobe(path: Path) -> tuple[int, int] | None:
    try:
        out = subprocess.run(
            [
                "ffprobe",
                "-v",
                "error",
                "-select_streams",
                "v:0",
                "-show_entries",
                "stream=width,height",
                "-of",
                "csv=p=0:s=x",
                str(path),
            ],
            capture_output=True,
            text=True,
            check=True,
            timeout=30,
        )
        s = out.stdout.strip()
        if "x" in s:
            w, h = s.split("x", 1)
            return int(w), int(h)
    except (FileNotFoundError, subprocess.CalledProcessError, ValueError, subprocess.TimeoutExpired):
        pass
    return None


def get_dimensions(path: Path) -> tuple[int, int] | None:
    return dims_via_identify(path) or dims_via_pillow(path) or dims_via_ffprobe(path)


def ratio_matches(w: int, h: int, rel_tol: float) -> bool:
    if h <= 0 or w <= 0:
        return False
    got = w / h
    return abs(got - REF_RATIO) / REF_RATIO <= rel_tol


def convert_to_bmp(magick_base: list[str], src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if magick_base[0] == "magick":
        cmd = [
            "magick",
            str(src),
            "-resize",
            f"{REF_W}x{REF_H}!",
            "-type",
            "TrueColor",
            "BMP3:" + str(dst),
        ]
    else:
        cmd = [
            "convert",
            str(src),
            "-resize",
            f"{REF_W}x{REF_H}!",
            "-type",
            "TrueColor",
            "BMP3:" + str(dst),
        ]
    subprocess.run(cmd, check=True, timeout=120)


def list_bmp_files(folder: Path) -> list[Path]:
    seen: set[str] = set()
    out: list[Path] = []
    for p in folder.iterdir():
        if not p.is_file():
            continue
        if p.suffix.lower() != ".bmp":
            continue
        key = p.name.lower()
        if key in seen:
            continue
        seen.add(key)
        out.append(p)
    return sorted(out, key=lambda x: x.name.lower())


def pad_width(n_files: int) -> int:
    return max(3, len(str(n_files)))


def folder_is_contiguous_numbered(folder: Path, slug: str) -> bool:
    """True if every bmp is slug_NNN.bmp and indices are 1..N with no gaps."""
    bmps = list_bmp_files(folder)
    if not bmps:
        return True
    pat = re.compile(rf"^{re.escape(slug)}_(\d+)\.bmp$", re.IGNORECASE)
    indices: list[int] = []
    for p in bmps:
        m = pat.match(p.name)
        if not m:
            return False
        indices.append(int(m.group(1)))
    indices.sort()
    return indices == list(range(1, len(indices) + 1))


def normalize_folder(
    folder: Path, slug: str, dry_run: bool, root: Path
) -> None:
    """Rename all *.bmp in folder to {slug}_001.bmp … in stable sorted order."""
    bmps = list_bmp_files(folder)
    if not bmps:
        return
    if folder_is_contiguous_numbered(folder, slug):
        return
    width = pad_width(len(bmps))
    # Two-phase rename to avoid clobbering
    temps: list[tuple[Path, Path]] = []
    for i, p in enumerate(bmps):
        tmp = folder / f".renorm_{i:05d}.tmp"
        temps.append((p, tmp))
    for src, tmp in temps:
        rel = src.relative_to(root)
        if dry_run:
            print(f"  mv {rel} -> {tmp.relative_to(root)} (temp)")
        else:
            src.rename(tmp)
    for i, (_, tmp) in enumerate(temps):
        final = folder / f"{slug}_{i + 1:0{width}d}.bmp"
        rel_t = tmp.relative_to(root)
        rel_f = final.relative_to(root)
        if dry_run:
            print(f"  mv {rel_t} -> {rel_f}")
        else:
            if final.exists():
                final.unlink()
            tmp.rename(final)


def normalize_all(characters_dir: Path, root: Path, dry_run: bool) -> int:
    """Renumber folders that are not already slug_001…slug_N. Returns folders changed."""
    if not characters_dir.is_dir():
        print(f"ERROR: {characters_dir}", file=sys.stderr)
        return 0
    changed = 0
    for sub in sorted(characters_dir.iterdir()):
        if not sub.is_dir() or sub.name.startswith("."):
            continue
        slug = sub.name
        bmps = list_bmp_files(sub)
        if not bmps:
            continue
        if folder_is_contiguous_numbered(sub, slug):
            continue
        print(f"normalize {slug}/ ({len(bmps)} bmp)")
        normalize_folder(sub, slug, dry_run, root)
        changed += 1
    return changed


def max_index_in_folder(folder: Path, slug: str) -> int:
    """Highest N in slug_N.bmp; 0 if none."""
    m = 0
    for p in folder.glob(f"{slug}_*.bmp"):
        mo = NUMBERED_RE.match(p.name)
        if mo and mo.group(1).lower() == slug.lower():
            m = max(m, int(mo.group(2)))
    return m


def scan_existing_ratios(characters_dir: Path, root: Path) -> dict[tuple[int, int], tuple[str, float]]:
    seen: dict[tuple[int, int], str] = {}
    for bmp in sorted(characters_dir.glob("*/*.bmp")):
        d = get_dimensions(bmp)
        if not d:
            continue
        w, h = d
        if (w, h) not in seen:
            seen[(w, h)] = str(bmp.relative_to(root))
    out: dict[tuple[int, int], tuple[str, float]] = {}
    for (w, h), rel in seen.items():
        out[(w, h)] = (rel, w / h)
    return out


def sort_bmp_names_for_slug(names: list[str], slug: str) -> list[str]:
    pat = re.compile(rf"^{re.escape(slug)}_(\d+)\.bmp$", re.IGNORECASE)

    def key(n: str) -> tuple[int, int | str]:
        mo = pat.match(n)
        if mo:
            return (0, int(mo.group(1)))
        return (1, n.lower())

    return sorted(names, key=key)


def collect_manifest(characters_dir: Path) -> dict[str, list[str]]:
    """Used by scripts/generate_wasm_character_manifest.py (importlib)."""
    out: dict[str, list[str]] = {}
    for sub in sorted(characters_dir.iterdir()):
        if not sub.is_dir() or sub.name.startswith("."):
            continue
        slug = sub.name
        names = [p.name for p in list_bmp_files(sub)]
        if not names:
            continue
        out[slug] = sort_bmp_names_for_slug(names, slug)
    return out


def cmd_import(args: argparse.Namespace, root: Path) -> int:
    tmp_dir = (args.tmp or root / "assets/tmp").resolve()
    ch_dir = (args.characters or root / "assets/characters").resolve()

    if not tmp_dir.is_dir():
        print(f"ERROR: tmp directory missing: {tmp_dir}", file=sys.stderr)
        return 1

    magick = find_magick()
    if not magick and not args.dry_run:
        print("ERROR: ImageMagick not found (`magick` or `convert`).", file=sys.stderr)
        return 1

    if not args.skip_normalize:
        print("Normalizing existing character BMP names …")
        n = normalize_all(ch_dir, root, args.dry_run)
        if n == 0:
            print("  (all folders already use slug_001.bmp, … naming)\n")
        else:
            print()

    print(
        f"Reference aspect: {REF_W}×{REF_H} (ratio {REF_RATIO:.6f} = 20∶27)\n"
    )
    if ch_dir.is_dir():
        ratios = scan_existing_ratios(ch_dir, root)
        if ratios:
            print("Existing asset sizes (sample paths):")
            for (w, h), (rel, r) in sorted(ratios.items(), key=lambda x: x[0]):
                print(f"  {w}×{h}  (ratio {r:.6f})  e.g. {rel}")
            print()

    files = sorted(
        list(tmp_dir.glob("*.png"))
        + list(tmp_dir.glob("*.PNG"))
        + list(tmp_dir.glob("*.jpg"))
        + list(tmp_dir.glob("*.JPG"))
        + list(tmp_dir.glob("*.jpeg"))
        + list(tmp_dir.glob("*.JPEG"))
    )

    skipped_ratio: list[tuple[Path, int, int, float]] = []
    skipped_name: list[Path] = []
    imported: list[tuple[str, Path]] = []
    next_idx: dict[str, int] = {}

    for src in files:
        m = STEM_RE.match(src.name)
        if not m:
            skipped_name.append(src)
            continue
        slug = m.group(1).lower()
        slug_dir = ch_dir / slug

        dims = get_dimensions(src)
        if not dims:
            print(f"SKIP (could not read dimensions): {src}", file=sys.stderr)
            continue
        w, h = dims
        r = w / h
        if not ratio_matches(w, h, args.aspect_rel_tol):
            skipped_ratio.append((src, w, h, r))
            print(
                f"SKIP aspect {w}×{h} (ratio {r:.6f}, need ~{REF_RATIO:.6f} "
                f"within rel tol {args.aspect_rel_tol}): {src.name}"
            )
            continue

        if slug not in next_idx:
            n = max_index_in_folder(slug_dir, slug) if slug_dir.is_dir() else 0
            next_idx[slug] = n + 1
        idx = next_idx[slug]
        next_idx[slug] = idx + 1

        prev_max = max_index_in_folder(slug_dir, slug) if slug_dir.is_dir() else 0
        width = max(3, len(str(max(prev_max, idx))))
        out_name = f"{slug}_{idx:0{width}d}.bmp"
        dst = slug_dir / out_name

        if args.dry_run:
            print(f"OK  {src.name} -> {dst.relative_to(root)}")
            imported.append((slug, dst))
            continue

        assert magick is not None
        slug_dir.mkdir(parents=True, exist_ok=True)
        convert_to_bmp(magick, src, dst)
        print(f"WROTE {dst.relative_to(root)}")
        imported.append((slug, dst))

    if skipped_name:
        print("\nSkipped (filename must be {slug}_{something}.png|.jpg):")
        for p in skipped_name:
            print(f"  {p.name}")

    if skipped_ratio:
        print(
            f"\nSkipped {len(skipped_ratio)} file(s) (aspect-rel-tol={args.aspect_rel_tol}):"
        )
        for src, w, h, r in skipped_ratio:
            print(f"  {src.name}  {w}×{h}  ratio {r:.6f}")

    if imported:
        print(
            "\nWeb build: run scripts/generate_wasm_character_manifest.py "
            "(or bash build-web.sh) so WASM picks up new BMPs.\n"
        )

    return 0


def cmd_normalize(args: argparse.Namespace, root: Path) -> int:
    ch_dir = (args.characters or root / "assets/characters").resolve()
    normalize_all(ch_dir, root, args.dry_run)
    return 0


def cmd_manifest(args: argparse.Namespace, root: Path) -> int:
    ch_dir = (args.characters or root / "assets/characters").resolve()
    out = root / "generated/wasm_character_manifest.cpp"
    subprocess.run(
        [
            sys.executable,
            str(root / "scripts/generate_wasm_character_manifest.py"),
            "--characters",
            str(ch_dir),
            "--out",
            str(out),
        ],
        check=True,
    )
    return 0


def main() -> int:
    root = repo_root()

    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = ap.add_subparsers(dest="cmd")

    p_norm = sub.add_parser("normalize", help="Renumber all character BMPs to slug_001.bmp …")
    p_norm.add_argument(
        "--characters",
        type=Path,
        default=None,
        help="Characters root (default: <repo>/assets/characters)",
    )
    p_norm.add_argument("--dry-run", action="store_true")
    p_norm.set_defaults(func=cmd_normalize)

    p_imp = sub.add_parser("import", help="Import from assets/tmp with sequential names")
    p_imp.add_argument("--tmp", type=Path, default=None)
    p_imp.add_argument("--characters", type=Path, default=None)
    p_imp.add_argument(
        "--aspect-rel-tol",
        type=float,
        default=0.012,
        metavar="TOL",
        help="Max relative aspect error (default: 0.012)",
    )
    p_imp.add_argument(
        "--skip-normalize",
        action="store_true",
        help="Do not renumber existing BMPs before assigning new indices",
    )
    p_imp.add_argument("--dry-run", action="store_true")
    p_imp.set_defaults(func=cmd_import)

    p_man = sub.add_parser(
        "manifest",
        help="Regenerate generated/wasm_character_manifest.cpp (same as build-web.sh step)",
    )
    p_man.add_argument("--characters", type=Path, default=None)
    p_man.set_defaults(func=cmd_manifest)

    argv = sys.argv[1:]
    subcmds = ("import", "normalize", "manifest")
    if not argv:
        argv = ["import"]
    elif argv[0] not in subcmds and argv[0] not in ("-h", "--help"):
        argv.insert(0, "import")

    args = ap.parse_args(argv)
    return args.func(args, root)


if __name__ == "__main__":
    sys.exit(main())
