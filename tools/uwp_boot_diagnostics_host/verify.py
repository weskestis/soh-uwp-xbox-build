#!/usr/bin/env python3
"""Run real Windows SEH/Store-CRT diagnostic cases and verify durable evidence."""
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

import pefile


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    executable = Path(sys.argv[1]).resolve()
    framework = Path(sys.argv[2]).resolve()
    evidence = Path(sys.argv[3]).resolve()
    # Store CRT DLLs are copied only into this isolated native test directory.
    with zipfile.ZipFile(framework) as archive:
        for name in archive.namelist():
            if name.lower().endswith('.dll') and '/' not in name and '\\' not in name:
                (executable.parent / name).write_bytes(archive.read(name))
    reports = []
    for name in ('diagnostics_driver.dll', 'diagnostics_fixture.dll'):
        binary = pefile.PE(str(executable.parent / name))
        imports = {entry.dll.decode().lower() for entry in binary.DIRECTORY_ENTRY_IMPORT}
        require(binary.OPTIONAL_HEADER.DllCharacteristics & 0x1000, name + ': missing AppContainer flag')
        require(any(dll.startswith('vcruntime') and dll.endswith('_app.dll') for dll in imports),
                name + ': not linked to the Store dynamic CRT')
        require(not imports & {'dbghelp.dll', 'psapi.dll'}, name + ': desktop diagnostic dependency')
        reports.append({'binary': name, 'imports': sorted(imports), 'store_crt_verified': True})
    evidence.mkdir(parents=True, exist_ok=True)
    for mode, operation in ((0, None), (1, 'read'), (2, 'write'), (3, 'execute'), (4, 'read'), (5, 'read'),
                            (6, 'storage blocked')):
        destination = evidence / f'case-{mode}'
        destination.mkdir(exist_ok=False)
        if mode == 1:
            (destination / 'logs').mkdir()
            (destination / 'logs' / 'existing.log').write_text('preserve existing engine logs\n')
        if mode == 6:
            (destination / 'logs').write_text('preserve conflicting user file\n')
        run = subprocess.run([str(executable), str(destination), str(mode)], cwd=executable.parent,
                             capture_output=True, timeout=30)
        require(run.returncode == (79 if mode == 6 else (23 if mode == 0 else 77)),
                f'case {mode}: exit {run.returncode}: {run.stderr.decode(errors="replace")}')
        boot = (destination / 'uwp-boot.log').read_text()
        out = (destination / 'uwp-stdout.log').read_text()
        err = (destination / 'uwp-stderr.log').read_text()
        addresses = json.loads((destination / 'addresses.json').read_text())
        require('state=open unbuffered=true' in boot, f'case {mode}: streams not captured')
        require('driver.stdout.shared-crt' in out and 'driver.stderr.shared-crt' in err,
                f'case {mode}: driver streams missing')
        if mode == 6:
            require('stage=storage.logs.failed win32=267' in boot and 'stage=core.descriptor.enter' not in boot,
                    'storage failure entered the core or was not diagnosed')
            require((destination / 'logs').read_text() == 'preserve conflicting user file\n',
                    'storage preflight overwrote a user file')
            print('native case 6: storage blocked PASS', flush=True)
            continue
        require((destination / 'logs').is_dir() and 'stage=storage.logs.ready' in boot,
                f'case {mode}: engine log directory not prepared')
        if mode == 1:
            require((destination / 'logs' / 'existing.log').read_text() == 'preserve existing engine logs\n',
                    'storage preflight overwrote an existing log')
        require('fixture.descriptor.entered' in err, f'case {mode}: separate Store DLL stderr missing')
        require('stage=core.descriptor.enter' in boot, f'case {mode}: no descriptor marker')
        require('stage=native-test.returned' in boot, f'case {mode}: filter did not return')
        if mode == 4:
            require('stage=core.descriptor.return' not in boot and 'stage=core.runner.enter' not in boot,
                    'descriptor fault misreported as runner fault')
        else:
            require('stage=core.descriptor.valid' in boot and 'stage=core.runner.enter' in boot,
                    f'case {mode}: missing runner markers')
            require('fixture.stdout.shared-crt' in out and 'fixture.stderr.shared-crt' in err,
                    f'case {mode}: separate Store DLL streams missing')
            require('fixture.stderr.win32-handle' in err,
                    f'case {mode}: Win32 stderr handle used by spdlog was not redirected')
        if mode == 0:
            require('stage=core.runner.return' in boot and 'stage=core.exception' not in boot,
                    'normal return misreported as fault')
        else:
            require('stage=core.exception code=0xC0000005' in boot, f'case {mode}: wrong fault code')
            require(f'crash.access={operation}' in boot and re.search(r'target=0*1\b', boot),
                    f'case {mode}: wrong access operation/target')
            require('stage=core.runner.return' not in boot, f'case {mode}: false successful runner return')
            frames = re.findall(r'crash.frame=(\d+) address=([0-9A-Fa-f]+) module=(\S+) base=([0-9A-Fa-f]+) rva=0x([0-9A-Fa-f]+)', boot)
            require(frames and len(frames) <= 24, f'case {mode}: no bounded fault-context stack')
            require(any(f[2].lower() == 'diagnostics_fixture.dll' for f in frames), f'case {mode}: fixture missing from stack')
            require(any(f[2].lower() == 'diagnostics_driver.dll' for f in frames), f'case {mode}: caller missing from stack')
            if mode == 5:
                pcs = [f[1] for f in frames if f[2].lower() == 'diagnostics_fixture.dll']
                require(any(pcs.count(pc) >= 3 for pc in pcs), 'recursive return PCs were truncated')
            for _, address, module, base, rva in frames:
                require(int(address, 16) - int(base, 16) == int(rva, 16), 'ASLR-relative address mismatch')
                if module.lower() == 'diagnostics_fixture.dll':
                    require(int(base, 16) == addresses['fixture_base'], 'wrong fixture base')
            if mode != 3:
                fault = re.search(r'crash.fault=0 .*module=diagnostics_fixture.dll .*rva=0x([0-9A-Fa-f]+)', boot)
                require(fault is not None, f'case {mode}: wrong fault module')
                start = addresses['write_rva' if mode == 2 else 'read_rva']
                require(start <= int(fault[1], 16) < start + 128, 'fault does not map to actual fixture function')
        print(f'native case {mode}: {operation or "normal return"} PASS', flush=True)
    (evidence / 'RESULT.json').write_text(json.dumps({'native_cases_passed': 7, 'storage_cases_passed': 3,
        'binaries': reports,
        'xbox_execution_verified': False}, indent=2) + '\n')


if __name__ == '__main__':
    main()
