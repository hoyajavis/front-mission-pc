#!/usr/bin/env python3
"""
lift_all_banks.py - Automated HiROM static lifting for Front Mission (SNES).
Consumes config/routines_to_lift.txt and generates partitioned C translation units
under src/gen/ (fm_bank_XX.c, fm_recomp.h, and fm_recomp_all.c).
"""

import collections
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../third_party/snesrecomp/tools"))
from disasm65816 import detect_mapping  # noqa: E402
from autogen import emit_function, Unsupported  # noqa: E402


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    rom_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(repo_root, "roms/Front Mission (Japan).sfc")
    manifest_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(repo_root, "config/routines_to_lift.txt")
    out_dir = os.path.join(repo_root, "src/gen")

    os.makedirs(out_dir, exist_ok=True)

    with open(rom_path, "rb") as rf:
        data = rf.read()
    if len(data) % 1024 == 512:
        data = data[512:]
    mapping = detect_mapping(data)
    print(f"Detected ROM mapping: {mapping} ({len(data)} bytes)")

    bank_entries = collections.defaultdict(list)
    total_manifest = 0
    with open(manifest_path, "r") as f:
        for line in f:
            line = line.split("#")[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) < 3:
                continue
            addr24 = int(parts[0], 16)
            P = int(parts[1], 16)
            is_long = (parts[2].upper() == "JSL")
            bank = (addr24 >> 16) & 0xFF
            bank_entries[bank].append((addr24, P, is_long))
            total_manifest += 1

    print(f"Total routines in manifest: {total_manifest} across {len(bank_entries)} banks")

    # Hardware / NMI / IRQ / DMA / Sound banks & routines that stay on LakeSnes interpreter
    EXCLUDED_BANKS = {0x00, 0x80, 0xC0}
    EXCLUDED_ROUTINES = set()

    candidates = {}
    skipped = {}

    for bank in sorted(bank_entries.keys()):
        for addr24, P, is_long in bank_entries[bank]:
            if bank in EXCLUDED_BANKS or addr24 in EXCLUDED_ROUTINES:
                skipped[addr24] = "system/NMI/IRQ/DMA bank (executes genuine on LakeSnes)"
                continue
            if (addr24 & 0xFFFF) < 0x8000 and (bank & 0x40) == 0:
                skipped[addr24] = "low RAM / sub-8000 mirror address (executes genuine on LakeSnes)"
                continue
            try:
                code = emit_function(data, addr24, P, is_long)
                targets = [int(t, 16) for t in re.findall(r"func_table_call(?:_jsr)?\(0x([0-9A-Fa-f]{6})u?\)", code)]
                sub8000 = [t for t in targets if (t & 0xFFFF) < 0x8000 and ((t >> 16) & 0x40) == 0]
                if sub8000:
                    skipped[addr24] = f"calls RAM/sub-8000 (${sub8000[0]:06X})"
                    continue
                all_calls = re.findall(r"func_table_call(?:_jsr)?\(([^)]+)\)", code)
                if any(not re.match(r"^0x[0-9A-Fa-f]{6}u?$", c.strip()) for c in all_calls):
                    skipped[addr24] = "indirect jump table dispatch"
                    continue
                if re.search(r"func_table_call(?:_jsr)?\(0x[0-9A-Fa-f]{6}u?\);\s*return;", code):
                    skipped[addr24] = "tail JML/JMP dispatch"
                    continue
                if re.search(r"bus_write.*0x43[0-7]", code) or re.search(r"bus_write.*0x420[0BCbc]", code):
                    skipped[addr24] = "DMA transfer / trigger routine"
                    continue
                if re.search(r"0x420[2-6]", code) or re.search(r"0x421[4-7]", code):
                    skipped[addr24] = "hardware math coprocessor ($4202-$4217)"
                    continue
                if re.search(r"0x214[0-3]", code):
                    skipped[addr24] = "APU sound communication ($2140-$2143)"
                    continue
                candidates[addr24] = {
                    "bank": bank,
                    "is_long": is_long,
                    "code": code,
                    "targets": targets,
                }
            except Unsupported as e:
                skipped[addr24] = str(e)

    # Iterative fixed-point pruning for closed call graph
    prune_pass = 1
    while True:
        to_prune = []
        for addr24, info in candidates.items():
            bad_callees = [t for t in info["targets"] if t not in candidates]
            if bad_callees:
                to_prune.append((addr24, f"calls unlifted target 0x{bad_callees[0]:06X}"))
        if not to_prune:
            break
        for addr24, why in to_prune:
            skipped[addr24] = why
            del candidates[addr24]
        prune_pass += 1

    total_lifted = len(candidates)
    total_skipped = len(skipped)
    active_banks = []

    for bank in sorted(bank_entries.keys()):
        done = [(a, c["is_long"], c["code"]) for a, c in candidates.items() if c["bank"] == bank]
        done.sort(key=lambda x: x[0])
        bank_skipped = [(a, skipped[a]) for a in skipped if ((a >> 16) & 0xFF) == bank]
        bank_skipped.sort(key=lambda x: x[0])

        if not done:
            continue

        active_banks.append(bank)
        bank_c_path = os.path.join(out_dir, f"fm_bank_{bank:02X}.c")
        with open(bank_c_path, "w", newline="\n") as f:
            f.write(f"/* Bank ${bank:02X} recompiled routines for Front Mission (SNES).\n")
            f.write(" * Generated automatically by tools/lift_all_banks.py. DO NOT COMMIT. */\n\n")
            f.write(f"/* Statistics: {len(done)} lifted, {len(bank_skipped)} skipped in Bank ${bank:02X} */\n")
            for a, why in bank_skipped[:20]:
                f.write(f"/* not lifted: ${a:06X}: {why} */\n")
            f.write("\n")
            f.write("#include <snesrecomp/snesrecomp.h>\n")
            f.write("#include <snesrecomp/gen_ops.h>\n")
            f.write('#include "fm_recomp.h"\n\n')
            f.write("#if defined(_MSC_VER)\n#pragma warning(disable: 4102)\n#endif\n\n")

            for _, _, code in done:
                f.write(code + "\n\n")

            f.write(f"void fm_gen_register_bank_{bank:02X}(bool with_intercept, bool with_validate) {{\n")
            f.write("    (void)with_intercept; (void)with_validate;\n")
            for addr24, is_long, _ in done:
                is_long_str = "true" if is_long else "false"
                f.write(f"    func_table_register(0x{addr24:06X}u, gen_{addr24:06X});\n")
                f.write(f"    if (with_intercept) recomp_timed_add_intercept(0x{addr24:06X}u, {is_long_str});\n")
                f.write(f"    if (with_validate) recomp_timed_add_validate(0x{addr24:06X}u, {is_long_str});\n")
            f.write("}\n")

        print(f"Bank ${bank:02X}: {len(done)} lifted, {len(bank_skipped)} skipped -> fm_bank_{bank:02X}.c")

    header_path = os.path.join(out_dir, "fm_recomp.h")
    with open(header_path, "w", newline="\n") as f:
        f.write("#ifndef FM_RECOMP_H\n#define FM_RECOMP_H\n\n")
        f.write("#include <stdbool.h>\n")
        f.write("#include <stdint.h>\n")
        f.write("#include <snesrecomp/func_table.h>\n\n")
        f.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
        for bank in active_banks:
            f.write(f"void fm_gen_register_bank_{bank:02X}(bool with_intercept, bool with_validate);\n")
        f.write("\nvoid fm_recomp_register_all(bool with_intercept, bool with_validate);\n")
        f.write("size_t fm_recomp_count(void);\n")
        f.write("const uint32_t *fm_recomp_addrs(void);\n\n")
        f.write("#ifdef __cplusplus\n}\n#endif\n\n")
        f.write("#endif /* FM_RECOMP_H */\n")

    all_c_path = os.path.join(out_dir, "fm_recomp_all.c")
    all_addrs = sorted(candidates.keys())
    with open(all_c_path, "w", newline="\n") as f:
        f.write("/* Master recompiled function dispatcher registration for Front Mission */\n")
        f.write('#include "fm_recomp.h"\n\n')
        f.write("static const uint32_t s_lifted_addrs[] = {\n")
        for a in all_addrs:
            f.write(f"    0x{a:06X}u,\n")
        if not all_addrs:
            f.write("    0u\n")
        f.write("};\n\n")
        f.write(f"size_t fm_recomp_count(void) {{ return {len(all_addrs)}u; }}\n")
        f.write("const uint32_t *fm_recomp_addrs(void) { return s_lifted_addrs; }\n\n")
        f.write("void fm_recomp_register_all(bool with_intercept, bool with_validate) {\n")
        for bank in active_banks:
            f.write(f"    fm_gen_register_bank_{bank:02X}(with_intercept, with_validate);\n")
        f.write("}\n")

    print("\n=======================================================")
    print(f"Lifting Complete: {total_lifted} lifted, {total_skipped} skipped")
    print(f"Active bank translation units: {len(active_banks)}")
    print("Master registration written to src/gen/fm_recomp_all.c")
    print("=======================================================\n")


if __name__ == "__main__":
    main()
