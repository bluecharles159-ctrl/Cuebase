#pragma once
#include "ElementDesign.h"
#include "CompositionGuides.h"
#include <functional>
#include <optional>

struct FrameExportOptions {
    QSize size{1920,1080};
    int fps=25, fpsBase=1;
    int firstFrame=0, endFrame=125; // End is exclusive, in scene time.
    RenderSettings renderSettings;
    std::optional<CompositionGuides::Proof> guideProof;
    QString colorView="Standard", colorLook="None";
    double exposure=0, gamma=1;
    bool setProjectFrameRate(int rate,double base);
};
struct FrameExportResult { bool success=false, cancelled=false; int written=0; QString error; };
// Writes straight-alpha PNG frames and a timing manifest into a new/empty directory.
// The scene is copied so UI edits cannot change an export in progress.
FrameExportResult exportSceneFrames(ElementDesign scene, const QString &directory,
    const FrameExportOptions &options, const std::function<bool(int,int)> &progress={});
struct Element;
FrameExportResult exportCueFrames(const Element &board,const QString &directory,
    const FrameExportOptions &options,const std::function<bool(int,int)> &progress={});
FrameExportResult exportCueMovie(const Element &board,const QString &file,const QString &encoder,
    const FrameExportOptions &options,const std::function<bool(int,int)> &progress={});
