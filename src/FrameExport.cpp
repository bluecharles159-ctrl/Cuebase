#include "FrameExport.h"
#include <QDir>
#include <QJsonDocument>
#include <QPainter>
#include <QSaveFile>
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

FrameExportResult exportSceneFrames(ElementDesign scene,const QString &directory,
    const FrameExportOptions &o,const std::function<bool(int,int)> &progress) {
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
    for(int frame=o.firstFrame;frame<o.endFrame;++frame) {
        if(progress&&!progress(result.written,count)){result.cancelled=true;return result;}
        QImage image(o.size,QImage::Format_ARGB32_Premultiplied);
        if(image.isNull())return fail("There is not enough memory for this frame size.");
        image.fill(Qt::transparent);
        {QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
         scene.render(painter,image.rect(),qRound64(double(frame)*1000*o.fpsBase/o.fps),true,nullptr,false);}
        QSaveFile file(output.filePath(QString("frame_%1.png").arg(frame,6,10,QChar('0'))));
        if(!file.open(QIODevice::WriteOnly)||!image.save(&file,"PNG")||!file.commit())
            return fail("Could not write frame "+QString::number(frame)+". Check free disk space and folder permissions.");
        ++result.written;
    }
    const QJsonObject manifest{{"format","Cuebase PNG sequence"},{"version",1},{"width",o.size.width()},
        {"height",o.size.height()},{"fps",o.fps},{"fpsBase",o.fpsBase},{"firstFrame",o.firstFrame},
        {"endFrameExclusive",o.endFrame},{"frames",result.written},{"pattern","frame_%06d.png"},
        {"alpha","straight"},{"audio",false}};
    QSaveFile file(output.filePath("sequence.json"));
    const auto data=QJsonDocument(manifest).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit())
        return fail("Frames were written, but the timing manifest could not be saved.");
    if(progress)progress(count,count);
    result.success=true;return result;
}
