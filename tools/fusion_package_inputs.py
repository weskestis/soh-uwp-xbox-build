#!/usr/bin/env python3
"""Assemble exact, audited Store artifacts for the Fusion diagnostic package."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import time
import urllib.error
import urllib.request
import zipfile

from uwp_runtime_stage import reject_private_files

REPO = "weskestis/soh-uwp-xbox-build"
ASSET_SOURCE = "3c4559d3a934cbb3598a4ab2c03122c3be1a5ffa"
DEPOT_SOURCE = "f5dbd58ee06a4b439bf260d641585dbc8bef3b86"
CORE_INPUTS = {
    "soh": (41260862534, "db701c752a7bcbe923416395c52f5cefe668810c"),
    "mm": (41246667942, "4bec8b52bdeb14f0a226c6fbea409fb6521f8e25"),
}
DEPOT_DLLS = ("SDL2.dll", "opengl32.dll", "libgallium_wgl.dll", "dxil.dll", "z-1.dll")
ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise RuntimeError(message)


def json_read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def api(path: str):
    request = urllib.request.Request(
        f"https://api.github.com/repos/{REPO}/{path}",
        headers={"Authorization": f"Bearer {os.environ['GH_TOKEN']}",
                 "Accept": "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def require_job(run_id: int, source: str, name: str, wait_seconds: int = 0):
    deadline = time.monotonic() + wait_seconds
    while True:
        run = api(f"actions/runs/{run_id}")
        require(run["head_sha"] == source and run["run_attempt"] == 1,
                f"Unexpected source or attempt for run {run_id}")
        jobs = api(f"actions/runs/{run_id}/jobs?per_page=100")["jobs"]
        selected = [job for job in jobs if job["name"] == name]
        require(len(selected) <= 1, f"Ambiguous native job: {name}")
        if selected and selected[0]["status"] == "completed":
            require(selected[0]["conclusion"] == "success", f"Native job failed: {name}")
            return run
        require(time.monotonic() < deadline, f"Required native job has not passed: {name}")
        print(f"Waiting for exact run {run_id}, {name}", flush=True)
        time.sleep(30)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def download_artifact(run_id: int, name: str, source: str, destination: Path):
    artifacts = api(f"actions/runs/{run_id}/artifacts?per_page=100")["artifacts"]
    matches = [a for a in artifacts if a["name"] == name and not a["expired"]]
    require(len(matches) == 1, f"Missing or ambiguous exact artifact {name}")
    artifact = matches[0]
    require(artifact["workflow_run"]["head_sha"] == source, "Artifact source mismatch")
    expected = artifact.get("digest", "").removeprefix("sha256:")
    require(re.fullmatch(r"[0-9a-f]{64}", expected) is not None, "Artifact lacks SHA-256")
    request = urllib.request.Request(artifact["archive_download_url"], headers={
        "Authorization": f"Bearer {os.environ['GH_TOKEN']}",
        "Accept": "application/vnd.github+json",
    })
    # Send the token only to GitHub; the redirected blob URL is independently signed.
    try:
        response = urllib.request.build_opener(NoRedirect).open(request, timeout=60)
    except urllib.error.HTTPError as error:
        if error.code not in (301, 302, 303, 307, 308):
            raise
        response = urllib.request.urlopen(error.headers["Location"], timeout=60)
    archive = destination.with_suffix(".zip")
    archive.parent.mkdir(parents=True, exist_ok=True)
    with response, archive.open("wb") as output:
        shutil.copyfileobj(response, output)
    require(sha256(archive) == expected, f"Artifact digest mismatch: {name}")
    destination.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(archive) as package:
        require(package.testzip() is None, "Corrupt artifact ZIP")
        seen = set()
        for member in package.infolist():
            path = PurePosixPath(member.filename)
            require(not path.is_absolute() and ".." not in path.parts and
                    "\\" not in member.filename and ":" not in member.filename,
                    "Unsafe artifact path")
            require(member.filename.casefold() not in seen, "Duplicate artifact path")
            require((member.external_attr >> 16) & 0o170000 != 0o120000, "Artifact symlink")
            seen.add(member.filename.casefold())
        package.extractall(destination)
    return {"id": artifact["id"], "name": name, "run": run_id, "attempt": 1,
            "source": source, "zip_sha256": expected}


def verify_core(core: str, directory: Path, source: str):
    reports = list(directory.rglob("store-audit.json"))
    require(len(reports) == 1, f"Missing native audit for {core}")
    report_dir = reports[0].parent
    report = json_read(reports[0])
    for flag in ("known_desktop_callbacks_absent", "store_crt_link_verified",
                 "mesa_import_abi_verified", "unsupported_crt_environment_absent"):
        require(report.get(flag) is True, f"Native {core} audit did not pass {flag}")
    metadata = dict(line.split("=", 1) for line in
                    (report_dir / "BUILD.txt").read_text(encoding="utf-8-sig").splitlines() if "=" in line)
    require(metadata.get("source_commit") == source, f"Native {core} provenance mismatch")
    binary = report_dir / f"{core}_core.dll"
    stamp = (report_dir / "SHA256.txt").read_text(encoding="utf-8-sig")
    match = re.search(r"Hash\s*:\s*([0-9A-Fa-f]{64})", stamp)
    require(match is not None and match.group(1).lower() == sha256(binary), f"Native {core} hash mismatch")
    return binary, report


def verify_port_archive(path: Path):
    with zipfile.ZipFile(path) as archive:
        require(archive.testzip() is None, f"Corrupt port archive: {path.name}")
        names = archive.namelist()
        require(len(names) > 10 and len(names) == len(set(names)), "Incomplete port archive")
        stamp = archive.read("portVersion")
        require(len(stamp) == 7 and stamp[0] in (0, 1), "Invalid portVersion")
        require(struct.unpack((">" if stamp[0] else "<") + "3H", stamp[1:]) == (9, 2, 3),
                "Port archive does not match the selected game-core version")


def reuse_oot_port():
    """Reuse the exact source/hash-verified archive from the signed 1.0.1.1 run."""
    source = "f7a886a3ab124f183181b30f8625f892855ba678"
    require_job(41247694255, source, "oot-port")
    destination = ROOT / "scratch/oot-port"
    record = download_artifact(41247694255, f"INTERNAL-fusion-oot-port-{source}", source, destination)
    require(record["id"] == 13059977713 and record["zip_sha256"] ==
            "b1404559f255f1c89497aba967991b152abb1118fdeb4a61d4d19876ad1c849d",
            "OoT port artifact differs from the audited installer input")
    require((destination / "SOURCE_COMMIT.txt").read_text().strip() == ASSET_SOURCE,
            "Reused OoT archive has a different asset source")
    require(sha256(destination / "soh.o2r") ==
            "4d0ebb4a7e20068dd68721dd1bae619f5812beb38c56af97593979a105e6b0b4",
            "Reused OoT archive differs from the signed 1.0.1.1 payload")
    verify_port_archive(destination / "soh.o2r")
    print("Exact OoT port source, artifact and signed-payload hashes verified")


def copy_assets(source: Path, runtime: Path):
    roots = {"Shipwright/libultraship/assets/rml": "assets/rml",
             "Shipwright/soh/assets/extractor": "assets",
             "2ship/assets/extractor": "assets"}
    seen = {}
    for origin, destination in roots.items():
        paths = subprocess.check_output(["git", "-C", str(source), "ls-files", "--", origin], text=True).splitlines()
        require(bool(paths), f"Missing tracked asset tree: {origin}")
        for name in paths:
            item = source / name
            require(item.is_file() and not item.is_symlink(), f"Invalid tracked asset: {name}")
            target_name = (Path(destination) / Path(name).relative_to(origin)).as_posix()
            digest = sha256(item)
            require(target_name.casefold() not in seen or seen[target_name.casefold()] == digest,
                    f"Conflicting game assets: {target_name}")
            seen[target_name.casefold()] = digest
            target = runtime / target_name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, target)


def main():
    os.chdir(ROOT)
    work = ROOT / "scratch/fusion-package-inputs"
    runtime = ROOT / "scratch/fusion-runtime"
    runtime.mkdir(parents=True, exist_ok=False)
    source = ROOT / "scratch/engine-assets-source"
    actual_source = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    require(actual_source == ASSET_SOURCE, "Unexpected host-asset source checkout")
    record = {"kind": "Fusion diagnostic boot test", "xbox_runtime_verified": False,
              "combined_randomizer": False, "shared_engine_handoff": False,
              "quest_layout_enforced": False, "wrapper_source": os.environ["GITHUB_SHA"],
              "package_run": os.environ["GITHUB_RUN_ID"], "cores": {}, "assets": {},
              "depot_source": DEPOT_SOURCE}
    for core, (run_id, core_source) in CORE_INPUTS.items():
        require_job(run_id, core_source, f"game-core ({core})", wait_seconds=5400)
        normal = work / f"{core}-core"
        normal_meta = download_artifact(run_id, f"EXPERIMENTAL-fusion-{core}-store-core-{core_source}", core_source, normal)
        diagnostics = work / f"{core}-diagnostics"
        diagnostics_meta = download_artifact(run_id, f"fusion-{core}-build-diagnostics-{run_id}-1", core_source, diagnostics)
        binary, audit = verify_core(core, diagnostics, core_source)
        require(sha256(normal / binary.name) == sha256(binary), "Core/evidence DLL mismatch")
        shutil.copy2(binary, runtime / binary.name)
        record["cores"][core] = {"source": core_source, "sha256": sha256(binary),
            "artifact": normal_meta, "diagnostics": diagnostics_meta,
            "audit": {key: value for key, value in audit.items() if isinstance(value, bool)}}
    require_job(41239071887, ASSET_SOURCE, "mm-port-archive")
    mm = work / "mm-assets"
    record["assets"]["mm"] = download_artifact(41239071887, f"INTERNAL-fusion-mm-port-archive-{ASSET_SOURCE}", ASSET_SOURCE, mm)
    for directory, name in ((mm, "2ship.o2r"), (ROOT / "scratch/oot-port", "soh.o2r")):
        require((directory / "SOURCE_COMMIT.txt").read_text().strip() == ASSET_SOURCE, "Port asset source mismatch")
        digest = sha256(directory / name)
        require((directory / "SHA256.txt").read_text().split()[0].lower() == digest, "Port asset hash mismatch")
        verify_port_archive(directory / name)
        shutil.copy2(directory / name, runtime / name)
        record["assets"][name] = {"source": ASSET_SOURCE, "sha256": digest}
    depot = ROOT / "scratch/uwp-dep"
    require(subprocess.check_output(["git", "-C", str(depot), "rev-parse", "HEAD"], text=True).strip() == DEPOT_SOURCE,
            "Unexpected Xbox dependency depot")
    record["depot_dlls"] = {name: sha256(depot / "x64/bin" / name) for name in DEPOT_DLLS}
    copy_assets(source, runtime)
    reject_private_files(runtime)
    (runtime / "assets/fusion-build.json").write_text(json.dumps(record, indent=2) + "\n")
    (work / "BUILD.json").write_text(json.dumps(record, indent=2) + "\n")
    print("Fresh Store cores, provenance and both ROM-free port archives verified")


if __name__ == "__main__":
    main()
