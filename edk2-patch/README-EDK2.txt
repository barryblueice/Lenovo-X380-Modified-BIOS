EDK2 UEFI payload 基础源码不在此目录（体积约 0.5 GB），它来自公开仓库：

  https://github.com/mrchromebox/edk2
  commit 1d840d4e6ed0e9a13fee47936c330f4f0cbf6510   (2026-07-01)

获取方式：

  git clone https://github.com/mrchromebox/edk2.git
  git -C edk2 checkout 1d840d4e6ed0e9a13fee47936c330f4f0cbf6510

然后把本目录 apps/ 下的 X380Autopatcher、X380EcUpdate 放到
UefiPayloadPkg/Application/ 下，并按 apps.tar.gz 的方式注入，再按
UefiPayloadPkg.dsc 编译。

项目自有的 payload 代码（不在上游仓库里的部分）都在 apps/ 和 apps.tar.gz 里。