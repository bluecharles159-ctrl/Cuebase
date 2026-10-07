# Local RTMP test receiver

Cuebase's RTMP regression test uses MediaMTX **1.21.1**. Download the Windows amd64 release from the [official release page](https://github.com/bluenviron/mediamtx/releases/tag/v1.21.1) and extract `mediamtx.exe` into this directory.

The executable is a local test dependency and is not stored in Git. The included MIT license belongs to MediaMTX. The tests generate their own temporary localhost configuration; the release's default configuration is not required.

The source ZIP packaging script also expects this executable to be present. Application-only builds do not require MediaMTX.
