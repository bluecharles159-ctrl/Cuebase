#include "FrameExport.h"
#include "CueSequence.h"
#include "Data.h"
#include "ColorManagement.h"
#include <QDir>
#include <QJsonDocument>
#include <QPainter>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QProcess>
#include <QFileInfo>
#include <QtMath>
#include <cmath>
#include <numeric>
#include <climits>

bool FrameExportOptions::setProjectFrameRate(int rate,double base) {
    fps=0;fpsBase=1;
    if(rate<1||!std::isfinite(base)||base<=0||base>rate||double(rate)/base>120)return false;
    const qint64 numerator=qint64(rate)*1000000,denominator=qRound64(base*1000000);
    if(denominator<1)return false;
    const auto divisor=std::gcd(numerator,denominator);
    if(numerator/divisor>INT_MAX||denominator/divisor>INT_MAX)return false;
    fps=int(numerator/divisor);fpsBase=int(denominator/divisor);return true;
}

static FrameExportResult writeFrames(const QString &directory,const FrameExportOptions &o,
    const std::function<bool(int,int)> &progress,
    const std::function<void(QPainter &,const QRect &,qint64,const RenderSettings &)> &paint) {
    FrameExportResult result;
    auto fail=[&](QString message){result.error=message;return result;};
    if(directory.trimmed().isEmpty()||o.size.width()<16||o.size.height()<16||
       o.size.width()>4096||o.size.height()>4096||o.fps<1||
       o.fpsBase<1||double(o.fps)/o.fpsBase>120||
       double(o.fps)/o.fpsBase<1||o.firstFrame<0||o.endFrame<=o.firstFrame||
       qint64(o.endFrame)-o.firstFrame>100000)
        return fail("Choose a valid size, frame rate and range (at most 100,000 frames).");
    QDir output(directory);
    if(output.exists()&&!output.entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System).isEmpty())
        return fail("The export folder must be empty. Existing files were preserved.");
    if(!QDir().mkpath(output.absolutePath()))return fail("Could not create the export folder.");
    const int count=o.endFrame-o.firstFrame;
    auto settings=o.renderSettings;
    settings.frameMs=1000.*o.fpsBase/o.fps;
    for(int frame=o.firstFrame;frame<o.endFrame;++frame) {
        if(progress&&!progress(result.written,count)){result.cancelled=true;return result;}
        QImage image(o.size,QImage::Format_ARGB32_Premultiplied);
        if(image.isNull())return fail("There is not enough memory for this frame size.");
        image.fill(Qt::transparent);
        {QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
         paint(painter,image.rect(),qRound64(double(frame)*1000*o.fpsBase/o.fps),settings);}
        if(colorManagementActive(o.colorView,o.colorLook,o.exposure,o.gamma))
            applyColorManagement(image,o.colorView,o.colorLook,o.exposure,o.gamma);
        if (o.guideProof) o.guideProof->draw(image);
        QSaveFile file(output.filePath(QString("frame_%1.png").arg(frame,6,10,QChar('0'))));
        if(!file.open(QIODevice::WriteOnly)||!image.save(&file,"PNG")||!file.commit())
            return fail("Could not write frame "+QString::number(frame)+". Check free disk space and folder permissions.");
        ++result.written;
    }
    const QJsonObject manifest{{"format","Cuebase PNG sequence"},{"version",1},{"width",o.size.width()},
        {"height",o.size.height()},{"fps",o.fps},{"fpsBase",o.fpsBase},{"firstFrame",o.firstFrame},
        {"endFrameExclusive",o.endFrame},{"frames",result.written},{"pattern","frame_%06d.png"},
        {"alpha","straight"},{"audio",false},{"compositionGuides",o.guideProof.has_value()}};
    QSaveFile file(output.filePath("sequence.json"));
    const auto data=QJsonDocument(manifest).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit())
        return fail("Frames were written, but the timing manifest could not be saved.");
    if(progress)progress(count,count);
    result.success=true;return result;
}

FrameExportResult exportSceneFrames(ElementDesign scene,const QString &directory,
    const FrameExportOptions &o,const std::function<bool(int,int)> &progress) {
    return writeFrames(directory,o,progress,[&](QPainter &p,const QRect &rect,qint64 time,const RenderSettings &settings) {
        scene.render(p,rect,time,true,&settings,false);
    });
}

FrameExportResult exportCueFrames(const Element &board,const QString &directory,
    const FrameExportOptions &o,const std::function<bool(int,int)> &progress) {
    const auto plan=compileCueBoard(ElementDesign::fromJson(board.design),board.cueBoard,board.cueBoardSettings);
    if(!plan.error.isEmpty() || plan.steps.isEmpty())
        return {false,false,0,plan.error.isEmpty() ? QString("Add a cue before exporting its animation.") : plan.error};
    QString lastDesign;
    ElementDesign scene;
    return writeFrames(directory,o,progress,[&](QPainter &p,const QRect &rect,qint64 time,const RenderSettings &settings) {
        const auto sample=plan.sample(time);
        if(!sample.visible) return;
        const auto &json=plan.steps[sample.step].design;
        if(lastDesign!=json){scene=ElementDesign::fromJson(json);lastDesign=json;}
        scene.render(p,rect,sample.frame,true,&settings,false);
    });
}

FrameExportResult exportCueMovie(const Element &board,const QString &file,const QString &encoder,
    const FrameExportOptions &options,const std::function<bool(int,int)> &progress) {
    if(encoder.isEmpty()) return {false,false,0,"The FFmpeg encoder could not be found."};
    const QFileInfo destination(file);
    QTemporaryDir staging(destination.absoluteDir().filePath(".cuebase-animation-XXXXXX"));
    if(!staging.isValid()) return {false,false,0,staging.errorString()};
    auto result=exportCueFrames(board,staging.path(),options,progress);
    if(!result.success) return result;
    const auto movie=staging.filePath("animation.mov");
    QProcess process;
    process.start(encoder,{"-nostdin","-hide_banner","-loglevel","error","-framerate",
        QString("%1/%2").arg(options.fps).arg(options.fpsBase),"-start_number",QString::number(options.firstFrame),
        "-i",staging.filePath("frame_%06d.png"),"-an","-c:v","qtrle","-pix_fmt","argb",movie});
    if(!process.waitForStarted()) return {false,false,0,process.errorString()};
    while(process.state()!=QProcess::NotRunning && !process.waitForFinished(100)) {
        if(progress && !progress(result.written,result.written)) {
            process.kill();process.waitForFinished();return {false,true,0,{}};
        }
    }
    if(process.exitStatus()!=QProcess::NormalExit || process.exitCode()!=0)
        return {false,false,0,"Movie encoding failed. "+QString::fromUtf8(process.readAllStandardError()).trimmed()};
    if(!destination.exists() && QFile::rename(movie,file)) return result;
    QFile input(movie);QSaveFile output(file);
    if(!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) return {false,false,0,"Could not write the animation file."};
    while(!input.atEnd()){
        if(progress && !progress(result.written,result.written)) return {false,true,0,{}};
        const auto block=input.read(1024*1024);
        if(input.error()!=QFile::NoError || output.write(block)!=block.size()) return {false,false,0,"Could not finish writing the animation file."};
    }
    if(!output.commit()) return {false,false,0,output.errorString()};
    return result;
}
