"""PlatformIO pre-build script: build the browser interface into the firmware.

Runs `npm run build` in ui/, folds the script and stylesheet into the one HTML
file, gzips it and writes src/generated/ui_bundle.h, which src/WebApi.cpp
serves at "/".

The interface is rebuilt only when something under ui/ has changed (a hash of
the sources is kept in the generated header), so firmware-only builds stay
fast and do not need Node at all once the header exists.

Node: the interface needs Node ^20.19 or >=22.12. If the `node` on PATH is too
old, a suitable version installed with nvm for Windows (NVM_HOME) is used
instead, without changing which version is the system default.
"""
import gzip
import hashlib
import os
import re
import shutil
import subprocess
import sys

Import("env")  # noqa: F821  (provided by PlatformIO)

PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
UI_DIR = os.path.join(PROJECT_DIR, "ui")
DIST_DIR = os.path.join(UI_DIR, "dist")
HEADER = os.path.join(PROJECT_DIR, "src", "generated", "ui_bundle.h")

# Everything that decides what the built interface contains.
SOURCE_FILES = ["index.html", "package.json", "package-lock.json", "vite.config.ts", "tsconfig.json"]
SOURCE_DIRS = ["src"]

HASH_MARK = "// ui-source-hash: "


def log(message):
    print(f"embed_ui: {message}")


def source_hash():
    paths = [os.path.join(UI_DIR, name) for name in SOURCE_FILES]
    for folder in SOURCE_DIRS:
        for root, _, files in os.walk(os.path.join(UI_DIR, folder)):
            paths.extend(os.path.join(root, name) for name in files)
    digest = hashlib.sha256()
    for path in sorted(paths):
        if not os.path.isfile(path):
            continue
        digest.update(os.path.relpath(path, UI_DIR).replace(os.sep, "/").encode())
        with open(path, "rb") as handle:
            # Ignore line-ending differences between checkouts.
            digest.update(handle.read().replace(b"\r\n", b"\n"))
    return digest.hexdigest()


def embedded_hash():
    try:
        with open(HEADER, encoding="utf-8") as handle:
            for line in handle:
                if line.startswith(HASH_MARK):
                    return line[len(HASH_MARK):].strip()
                if not line.startswith("//"):
                    break
    except OSError:
        pass
    return None


def node_version(node_dir):
    executable = shutil.which("node", path=node_dir)
    if not executable:
        return None
    try:
        text = subprocess.run([executable, "--version"], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    match = re.match(r"v(\d+)\.(\d+)\.(\d+)", text.strip())
    return tuple(int(part) for part in match.groups()) if match else None


def version_ok(version):
    """Vite's requirement: ^20.19.0 || >=22.12.0."""
    if version is None:
        return False
    major, minor, _ = version
    return (major == 20 and minor >= 19) or (major == 22 and minor >= 12) or major >= 23


def find_node_dir():
    """Folder holding a suitable node and npm, or None."""
    candidates = []
    on_path = shutil.which("node")
    if on_path:
        candidates.append(os.path.dirname(on_path))
    nvm_home = os.environ.get("NVM_HOME")
    if nvm_home and os.path.isdir(nvm_home):
        versions = []
        for name in os.listdir(nvm_home):
            match = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)", name)
            if match:
                versions.append((tuple(int(part) for part in match.groups()), os.path.join(nvm_home, name)))
        candidates.extend(path for _, path in sorted(versions, reverse=True))
    for folder in candidates:
        if version_ok(node_version(folder)):
            return folder
    return None


def run_npm(node_dir, *arguments):
    npm = shutil.which("npm", path=node_dir)
    if not npm:
        raise RuntimeError(f"npm was not found beside node in {node_dir}")
    child_env = dict(os.environ)
    child_env["PATH"] = node_dir + os.pathsep + child_env.get("PATH", "")
    log("npm " + " ".join(arguments))
    result = subprocess.run([npm, *arguments], cwd=UI_DIR, env=child_env)
    if result.returncode != 0:
        raise RuntimeError(f"npm {' '.join(arguments)} failed (exit {result.returncode})")


def install_packages_if_needed(node_dir):
    """npm ci when node_modules is missing or was installed from another lock file."""
    lock = os.path.join(UI_DIR, "package-lock.json")
    stamp = os.path.join(UI_DIR, "node_modules", ".eye-lock-hash")
    with open(lock, "rb") as handle:
        lock_hash = hashlib.sha256(handle.read().replace(b"\r\n", b"\n")).hexdigest()
    try:
        with open(stamp, encoding="utf-8") as handle:
            if handle.read().strip() == lock_hash:
                return
    except OSError:
        pass
    run_npm(node_dir, "ci", "--no-fund", "--no-audit")
    with open(stamp, "w", encoding="utf-8") as handle:
        handle.write(lock_hash)


def read_asset(reference):
    path = os.path.normpath(os.path.join(DIST_DIR, reference.lstrip("/")))
    if not path.startswith(DIST_DIR + os.sep):
        raise RuntimeError(f"asset path leaves the build folder: {reference}")
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def single_file_html():
    """dist/index.html with its stylesheet and script written into the page."""
    with open(os.path.join(DIST_DIR, "index.html"), encoding="utf-8") as handle:
        html = handle.read()

    def inline_style(match):
        return "<style>" + read_asset(match.group(1)) + "</style>"

    def inline_script(match):
        # A literal </script> inside the code would end the element early.
        code = read_asset(match.group(1)).replace("</script", "<\\/script")
        return '<script type="module">' + code + "</script>"

    html, styles = re.subn(r'<link\b[^>]*\brel="stylesheet"[^>]*\bhref="([^"]+)"[^>]*>', inline_style, html)
    html, scripts = re.subn(r'<script\b[^>]*\bsrc="([^"]+)"[^>]*>\s*</script>', inline_script, html)
    if scripts != 1 or styles > 1:
        raise RuntimeError(f"expected one script and at most one stylesheet, found {scripts} and {styles}")
    if re.search(r'\b(?:src|href)="/assets/', html):
        raise RuntimeError("the built page still refers to a separate asset file")
    return html.encode("utf-8")


def write_header(page, sources):
    # mtime=0 so the same page always gives the same bytes.
    packed = gzip.compress(page, compresslevel=9, mtime=0)
    etag = hashlib.sha256(packed).hexdigest()[:16]

    lines = [
        HASH_MARK + sources,
        "// Generated by scripts/embed_ui.py from ui/. Do not edit; do not commit.",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        f"// The interface: one HTML page, gzip-compressed ({len(page)} bytes before, {len(packed)} after).",
        f"constexpr size_t UI_BUNDLE_GZ_LEN = {len(packed)};",
        f'constexpr char UI_BUNDLE_ETAG[] = "\\"{etag}\\"";',
        "const uint8_t UI_BUNDLE_GZ[] = {",
    ]
    for offset in range(0, len(packed), 20):
        lines.append("  " + ",".join(f"0x{byte:02x}" for byte in packed[offset:offset + 20]) + ",")
    lines.append("};")
    lines.append("")

    os.makedirs(os.path.dirname(HEADER), exist_ok=True)
    with open(HEADER, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))
    return len(page), len(packed)


def main():
    sources = source_hash()
    if embedded_hash() == sources:
        log("interface unchanged; using the existing bundle")
        return

    node_dir = find_node_dir()
    if node_dir is None:
        message = (
            "the interface needs Node ^20.19 or >=22.12, and none was found on PATH or under NVM_HOME. "
            "Install one (for example: nvm install 24) and build again."
        )
        if os.path.isfile(HEADER):
            log("WARNING: " + message)
            log("WARNING: building with the OLD interface bundle; changes under ui/ are not included")
            return
        sys.stderr.write("embed_ui: " + message + "\n")
        env.Exit(1)  # noqa: F821

    version = ".".join(str(part) for part in node_version(node_dir))
    log(f"interface changed; building with Node {version} from {node_dir}")
    try:
        install_packages_if_needed(node_dir)
        run_npm(node_dir, "run", "build")
        raw, packed = write_header(single_file_html(), sources)
    except (RuntimeError, OSError) as error:
        sys.stderr.write(f"embed_ui: {error}\n")
        env.Exit(1)  # noqa: F821
    log(f"bundle written: {raw} bytes, {packed} bytes compressed")


# IDE project indexing runs the scripts too; there is nothing to build then.
if not env.IsIntegrationDump():  # noqa: F821
    main()
