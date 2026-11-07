# This is an unofficial project for cracking / reverse-engineering Lenovo X380 Yoga BIOS

Base on newest BIOS version [v1.41 from Lenovo Official](https://pcsupport.lenovo.com/us/en/products/laptops-and-netbooks/thinkpad-x-series-laptops/thinkpad-x380-yoga/downloads/driver-list/component?name=BIOS%2FUEFI&id=5AC6A815-321D-440E-8833-B07A93E0428C).
 Includes official modified & coreboot.

### Current supported model:

Supported model | Notes
| :-: | :-: |
Lenovo ThinkPad X380 Yoga (Black) | Global Region
Lenovo ThinkPad S1 2018 (Silver) | CN Region

> [!CAUTION]
> All firmware need to use SPI programmer to burn in.<br>
> Burning modified firmware is a risky operation that may cause hardware damage. If you are afraid to brick your device, **DON'T DO IT**.

> [!WARNING]
> Not Recommend use coreboot BIOS as daily use. Deguard will cause some sensors mounted under ISH may not worked. For example:
>
> - Sensors on LCD (maybe Environmental light sensor and lid closure sensor)
> - KXCJ9-1008 Accelerometer
> - KX023-1025 Accelerometer
> - KB9022 PS2 Interface
> - NFC Module
> - Smart Card Module
> - LOGO LED
>
>These functions may work abnormally:
> - Gravity-sensing screen rotation
> - NFC & Smart Card Insert detected
> - Breathing light under S3/S4
> - LCD Cover/Flip Cover Detection

***

### Support Function:

| Function | Notes | BIOS Model |
| :-: | :-: | :-: |
| Unlock BIOS Advanced Setting | / | Official Modified |
| Remove WWAN PCIE limitation | / | Official Modified |
| EC Auto-Patcher | Bypass / clear BIOS Supervisor Password storaged in EC (Embedded Controller) <br>EC Model: ENC KB9022, reverse-engineering communication from ENC KB9012 | Coreboot |
| EC Updater | Update EC firmware only | Coreboot |
| Deguard | [Intel BootGuard bypass on Skylake/Kabylake](https://doc.coreboot.org/soc/intel/deguard.html) | Coreboot

***

### How to build Official Modified Firmware (Under Windows):

1. Use SPI Programmer to extract your BIOS firmware:
   
<img width="731" height="480" alt="775E7448E357671BF7122B07A873BE1B" src="https://github.com/user-attachments/assets/d082ccb5-f720-4903-b6bb-f099ce3823ae" />

2. Use command belowed to patch:

```
# Python
python scripts\build.py --input a.bin --mode both

# PowerShell
.\build.ps1 -InputImage a.bin -Mode both
```

3. Use SPI Programmer to burn patched BIOS firmware.

***

### How to build Coreboot (Under Linux):

1. Prepare build dependency:

```

# Ubuntu Linux

sudo apt update

sudo apt install -y build-essential bison flex git m4 pkg-config \
    libncurses-dev libssl-dev libelf-dev zlib1g-dev uuid-dev \
    nasm acpica-tools python3 python3-pip \
    gcc-multilib g++-multilib libc6-dev-i386 \
    gnat

git clone https://github.com/coreboot/coreboot

cd coreboot

make crossgcc-i386 CPUS=$(nproc)

cd coreboot/payloads/external/edk2

mkdir -p workspace && cd workspace

git clone https://github.com/mrchromebox/edk2.git mrchromebox

```

2. Copy edk2 Patched file:

```
tar -xzf edk2-patched/apps.tar.gz -C coreboot/mrchromebox/UefiPayloadPkg/Application
ls coreboot/mrchromebox/UefiPayloadPkg/Application

```

2. Configured coreboot make option:

```
make menuconfig
```

3. Edited `mrchromebox/UefiPayloadPkg/UefiPayloadPkg.dsc`:

```
# Add to end-of-file:

UefiPayloadPkg/Application/X380Autopatcher/X380Autopatcher.inf
UefiPayloadPkg/Application/X380EcUpdate/X380EcUpdate.inf
```

4. Edited `mrchromebox/UefiPayloadPkg/UefiPayloadPkg.fdf`:

```
# Add to end-of-file:

INF UefiPayloadPkg/Application/X380Autopatcher/X380Autopatcher.inf
INF UefiPayloadPkg/Application/X380EcUpdate/X380EcUpdate.inf
```
5. Edited `mrchromebox/UefiPayloadPkg/Library/PlatformBootManagerLib/PlatformBootManager.c`, add below code before `// Register iPXE ...`:

```
  {
    EFI_GUID EcGuid = {0xc140da7e,0x722b,0x4c81,{0xa3,0x83,0x58,0x23,0x80,0xa0,0x57,0x03}};
    PlatformRegisterFvBootOption (&EcGuid, L"X380 EC update (manual)",
                                  LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_APP);
  }
  {
    EFI_GUID ApGuid = {0xc140da7e,0x722b,0x4c81,{0xa3,0x83,0x58,0x23,0x80,0xa0,0x57,0x01}};
    PlatformRegisterFvBootOption (&ApGuid, L"X380 Autopatcher (manual)",
                                  LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_APP);
  }
```
The GUID must be completed the same as the FILE_GUID in the .inf file (EC = …5703, Autopatcher = …5701).

6. Build EDK2 payload:

```
cd coreboot/payloads/external/edk2/workspace
export WORKSPACE="$PWD"
export PACKAGES_PATH="$PWD/mrchromebox"
export EDK_TOOLS_PATH="$PWD/mrchromebox/BaseTools"
source mrchromebox/edksetup.sh

build -a IA32 -a X64 -b RELEASE \
      -p UefiPayloadPkg/UefiPayloadPkg.dsc -t GCC -q -s \
      -D BUILD_ARCH=X64 \
      -D VARIABLE_SUPPORT=SMMSTORE \
      -D BOOTLOADER=COREBOOT
```

Expected output:

```
Build/UefiPayloadPkgX64/RELEASE_GCC/FV/UEFIPAYLOAD.fd
Build/UefiPayloadPkgX64/RELEASE_GCC/X64/UefiPayloadPkg/Application/X380EcUpdate/X380EcUpdate/OUTPUT/X380EcUpdate.efi
Build/UefiPayloadPkgX64/RELEASE_GCC/X64/UefiPayloadPkg/Application/X380Autopatcher/X380Autopatcher/OUTPUT/X380Autopatcher.efi
```

7. Testing （Optional）:

```
cd coreboot/payloads/external/edk2/workspace/mrchromebox
gcc -fshort-wchar -ffunction-sections -fdata-sections \
    -I MdePkg/Include -I MdePkg/Include/X64 \
    UefiPayloadPkg/Application/X380Autopatcher/test_host.c \
    UefiPayloadPkg/Application/X380Autopatcher/autopatcher.c \
    -Wl,--gc-sections -o /tmp/test-autopatcher && /tmp/test-autopatcher

gcc -O2 -fshort-wchar -I MdePkg/Include -I MdePkg/Include/X64 \
    UefiPayloadPkg/Application/X380EcUpdate/test_ec.c \
    UefiPayloadPkg/Application/X380EcUpdate/ec_flash.c -o /tmp/test-ec && /tmp/test-ec
```

8. Build coreboot:

```
cd coreboot
export PATH="$PWD/util/crossgcc/xgcc/bin:$PATH"
make olddefconfig UPDATED_SUBMODULES=1
make -j$(nproc) UPDATED_SUBMODULES=1
```

Expected output:

```
build/coreboot.rom
```

***

### About IBB Disguise

Due to Intel® Boot Guard function, coreboot need to rely on Deguard function.<br>
If coreboot started from Deguard ME, ISH (Intel® Integrated Sensor Hub) will disabled. Many sensors mounted on ISH also not working.<br>
In theory we can extract and simulate IBB from original OEM BIOS, as a disguise to deceive Intel® Boot Guard, work as a "legal" firmware.<br>
I can't finish this job due to some limitation, I hope that those who are involved in this project can try this theory. :)

References:

1. [Intel BootGuard bypass on Skylake/Kabylake](https://doc.coreboot.org/soc/intel/deguard.html)
2. [Lenovo BIOS AutoPatcher](https://github.com/suds-mcduff/lenovo-bios-autopatcher)
3. [ThinkPad Firmware Patches](https://github.com/digmorepaka/thinkpad-firmware-patches)
