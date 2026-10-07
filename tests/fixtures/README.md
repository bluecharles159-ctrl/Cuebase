# Regression fixtures

`cube.fbx` and `cube.glb` were exported from Blender 5.1.0's factory cube for Cuebase's mesh-import tests. `material.mp4` is a two-second FFmpeg test pattern with a generated 440 Hz tone. These fixtures contain no user media.

Streaming tests create their own temporary tone and local receivers. They do not contact a public broadcast endpoint. MediaMTX 1.21.1 is the RTMP receiver; its executable and MIT license are in `tools/mediamtx`.
