from pathlib import Path
import json

r = Path(__file__).resolve().parent
p = r.parent
report = json.loads((r / 'validation.json').read_text())
tests = json.loads((r / 'script-validation.json').read_text())
delivery = json.loads((r / 'usb-delivery.json').read_text(encoding='utf-8-sig'))
assert len(tests) == 6 and all(t['status'] == 'passed' for t in tests)
assert delivery['files']['coreboot.bin'] == report['sha256']
assert not delivery['executed_flash_script']
report.update(
    status='BUILT_VALIDATED_DELIVERED_NOT_FLASHED',
    linux_script_mock_tests_passed=6,
    usb_delivery=delivery,
    currently_programmed=False,
    delivery_directory=str(r),
    user_confirmed_working_baseline='coreboot-vendor-refresh BIOS and e29d893b USB EC updater',
    ec_hardware_evidence='evidence/ec-runtime-success.json',
)
for path in [r / 'validation.json', p / 'latest-built-image.json']:
    path.write_text(json.dumps(report, indent=2) + '\n')
f = p / 'AGENTS.md'
s = f.read_text()
marker = '- INTEGRATED EC RELEASE (2026-11-02):'
if marker not in s:
    text = (marker + ' `coreboot-ec-integrated/coreboot.bin`, SHA256 `'
            + report['sha256'] + '`, contains the successful USB EC engine unchanged, integrated as manual APP GUID 5703. '
            'User confirmed all EC postboot functions normal before integration. Full model, real-dump preservation/repeat-skip tests, inspection-only tests, Autopatcher guards and six mocked BIOS-script cases passed. '
            'Final ROM extracted payload contains exact newly compiled EC and Autopatcher EFI bytes; working prefix/ME, GOP/VBT/SPD preserved. '
            'Delivered and hash-verified at L:\\coreboot.bin with flash.sh, backup.sh and SHA256SUMS.txt; old USB tools/readback archived at `'
            + delivery['archive'] + '`. New BIOS NOT flashed or hardware tested. Current flashed BIOS remains vendor-refresh be448f33...; latest-programmed-image.json is intentionally unchanged. '
            'Entry: ESC -> Boot Manager -> X380 EC update (manual), I inspect, U then UPDATE; same target skips writes. Earlier pending/USB-only notes below are historical.\n\n')
    s = s.replace('# Firmware delivery preferences\n\n', '# Firmware delivery preferences\n\n' + text, 1)
    f.write_text(s, newline='\n')
print(json.dumps({k: report[k] for k in ['status', 'sha256', 'flashed', 'linux_script_mock_tests_passed']}, indent=2))
