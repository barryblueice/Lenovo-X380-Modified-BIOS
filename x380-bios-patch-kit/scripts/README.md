# Scripts

All scripts are plain Python 3 (tested with 3.11). The analysis scripts need
`capstone`:

```
pip install capstone
```

## build.py — build a patched image

```
python build.py --input a.bin --mode both
python build.py --input a.bin --mode wwan --out-dir dist
```

Modes:

| mode | patch file | result |
|---|---|---|
| `both` | `x380_custom_patches.txt` | Advanced menu + WWAN bypass |
| `adv` | `adv_only.txt` | Advanced menu only |
| `wwan` | `wwan_only.txt` | WWAN bypass only |

What it does:

1. checks the image is 16 MiB and has the Flash Descriptor signature at `0x10`;
2. reads the BIOS ID and refuses anything other than `R0SET57W` unless `--force`;
3. runs `tools/UEFIPatch_0.28.0/UEFIPatch.exe` with the selected patch file;
4. counts the replacements and fails if the count is not what the mode expects;
5. optionally replaces the `LNVBBSEC` marker byte `FB` with `FF` if present
   (`--lnvbb auto` is the default; on 1.41 it is already `FF`, so nothing changes);
6. writes the image plus a `.json` manifest and a `.sha256` file.

It never flashes anything.

Options:

```
--out-dir DIR      output directory (default: <kit>/dist)
--uefipatch PATH   path to UEFIPatch.exe
--lnvbb {auto,off} marker handling
--force            continue on unexpected size / BIOS ID
```

## verify_patch.py — check a built image

```
python verify_patch.py --image dist/x380-R0SET57W-adv-wwan.bin --mode both
```

It extracts the image with UEFIExtract (full tree) and checks:

- size is 16 MiB;
- `LenovoWmaPolicyDxe` PE32 at RVA `0x684` equals `31 c0 e9 ad 03 00 00`;
- `SystemFormBrowserCoreDxe` contains the new formset GUID
  `778b1d82-6d24-964e-8e10-3467d56ab1ba`.

UEFIExtract is not bundled with the kit. Download UEFITool NE and either place
`UEFIExtract.exe` at `tools/UEFIExtract_NE_A76_win64/` or pass
`--uefiextract <path>`.

Note: on some Windows setups UEFIExtract cannot create directories under the
system temp, so the script uses a temporary directory inside the kit by default
(`--work-dir` to change).

## Analysis scripts

These were used to derive the patch and can be reused on another BIOS version.

| script | purpose |
|---|---|
| `pe_info.py` | PE header, sections, entry point, key strings |
| `disasm.py` | disassemble `.text`, list RIP-relative references |
| `dis_range.py` | disassemble a specific address range |
| `wma_funcs.py` | function starts, data references, caller map |
| `wma_dump.py` | dump data, qword pointers, a fixed disassembly range |

Typical sequence:

```
python pe_info.py body.bin
python disasm.py body.bin
python wma_funcs.py body.bin
python dis_range.py body.bin 0x5d4 0x760
```

## build.ps1

Windows wrapper for `build.py`:

```
.\build.ps1 -InputImage a.bin -Mode both
.\build.ps1 -InputImage a.bin -Mode wwan -OutDir .\dist
```
