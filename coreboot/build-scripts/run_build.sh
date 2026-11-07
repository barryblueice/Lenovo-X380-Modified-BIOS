set -euo pipefail
work="$HOME/codex-x380-edk2-cf7c35d"
cd "$work/src"
export PATH="$PWD/util/crossgcc/xgcc/bin:$PATH"
if [ -f "$work/build.exit" ]; then mv "$work/build.exit" "$work/build.exit.previous.$(date +%s)"; fi
(
    set +e
    (
        set -e
        date -u
        git -c http.lowSpeedLimit=1024 -c http.lowSpeedTime=60 submodule update --init --checkout --depth 1 --jobs 6 -- 3rdparty/blobs 3rdparty/fsp 3rdparty/libgfxinit 3rdparty/libhwbase 3rdparty/vboot 3rdparty/intel-microcode
        git submodule status 3rdparty/blobs 3rdparty/fsp 3rdparty/libgfxinit 3rdparty/libhwbase 3rdparty/vboot 3rdparty/intel-microcode
        make -j12 UPDATED_SUBMODULES=1
        printf '\nBuild artifacts:\n'
        sha256sum build/coreboot.rom build/UEFIPAYLOAD.fd
        build/util/cbfstool/cbfstool build/coreboot.rom print
        build/util/cbfstool/cbfstool build/coreboot.rom layout
        date -u
    )
    code=$?
    printf '%s\n' "$code" > "$work/build.exit"
    exit "$code"
) > "$work/build.log" 2>&1 < /dev/null &
echo "Build PID: $!"
