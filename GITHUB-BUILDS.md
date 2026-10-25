# Cuebase builds on GitHub

The Windows workflow compiles on a fresh `windows-2022` GitHub runner using
MSYS2 UCRT64. Your PC only edits/uploads source and downloads the resulting ZIP.
No local compiler installation or user library is uploaded.

## What runs

1. Install/cache Qt, GCC, Assimp, Bullet, FFmpeg, CMake and Ninja.
2. Compile the app, renderer and all regression executables with two workers.
3. Run the CTest suites serially: Build, planning, textures, components, editor
   interactions and Studio/streaming. Localhost RTMP uses a checksum-pinned
   MediaMTX receiver; no stream is sent to a public service.
4. Run the interaction suite with the native Windows platform plugin.
5. Bundle the runtime and complete source. Audit recursive DLL imports, then
   run the packaged tests with MSYS2 removed from PATH.
6. Produce a ZIP with SHA-256 file manifest only if the required checks pass.
   Failed runs still upload diagnostic reports and screenshots.

Pushes to `codex/cloud-build` run automatically. The workflow also supports pull
requests into main and manual runs once available on the default branch.
Newer pushes cancel obsolete runs on the same branch. Compiler/dependency caches
reduce repeat work; the first dependency install and full build still take time.
Artifacts are retained for 14 days. GitHub account Actions quotas apply.

Open the repository's **Actions** tab, select **Windows build, tests and portable
ZIP**, and choose a successful run. Download its **Cuebase-Windows-N** artifact,
then extract the contained app ZIP and run **Start Cuebase.cmd**.

This verifies functionality on an independent Windows environment. It does not
certify every GPU, audio device, capture card, broadcaster or operating system.
macOS/Linux release builds and the remaining product acceptance groups are still
tracked separately. The workflow never publishes a GitHub Release automatically.

References: [MSYS2 setup action](https://github.com/msys2/setup-msys2),
[GitHub hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
