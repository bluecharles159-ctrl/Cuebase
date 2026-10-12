#pragma once
#include "ElementDesign.h"
#include <functional>

struct FrameExportOptions {
    QSize size{1920,1080};
    int fps=25, fpsBase=1;
    int firstFrame=0, endFrame=125; // End is exclusive, in scene time.
};
struct FrameExportResult { bool success=false, cancelled=false; int written=0; QString error; };
// Writes straight-alpha PNG frames and a timing manifest into a new/empty directory.
// The scene is copied so UI edits cannot change an export in progress.
FrameExportResult exportSceneFrames(ElementDesign scene, const QString &directory,
    const FrameExportOptions &options, const std::function<bool(int,int)> &progress={});
