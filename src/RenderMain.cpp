#include "FrameExport.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextStream>
#include <cmath>

int main(int argc,char **argv) {
    QApplication app(argc,argv);QCoreApplication::setApplicationName("CuebaseRender");
    QCommandLineParser parser;parser.setApplicationDescription("Render a Cuebase composition to PNG frames with alpha. Audio is not included.");parser.addHelpOption();
    parser.addPositionalArgument("scene","Input .cuebuild composition");parser.addPositionalArgument("directory","New or empty output folder");
    for(const auto &option:QList<QCommandLineOption>{{"width","Frame width","pixels","1920"},{"height","Frame height","pixels","1080"},
        {"fps","Frame-rate numerator","rate","25"},{"fps-base","Frame-rate denominator","base","1"},
        {"first-frame","First scene frame","frame","0"},{"end-frame","Exclusive last frame (default: scene end)","frame"}})parser.addOption(option);
    parser.process(app);const auto args=parser.positionalArguments();if(args.size()!=2)parser.showHelp(2);
    QTextStream error(stderr),log(stdout);QFile file(args[0]);
    if(!file.open(QIODevice::ReadOnly)||file.size()>64*1024*1024){error<<"Cannot read composition (maximum 64 MiB).\n";return 2;}
    const auto data=file.readAll();QJsonParseError parseError;const auto json=QJsonDocument::fromJson(data,&parseError);
    if(parseError.error!=QJsonParseError::NoError||!json.isObject()||!json.object()["objects"].isArray()){error<<"Invalid Cuebase composition.\n";return 2;}
    auto scene=ElementDesign::fromJson(QString::fromUtf8(data));
    for(auto &object:scene.objects)if(!object.image.isEmpty()&&QFileInfo(object.image).isRelative())object.image=QFileInfo(args[0]).absoluteDir().absoluteFilePath(object.image);
    FrameExportOptions options;bool valid=true;
    auto integer=[&](const char *name){bool ok=false;const int value=parser.value(name).toInt(&ok);valid&=ok;return value;};
    options.size={integer("width"),integer("height")};options.fps=integer("fps");options.fpsBase=integer("fps-base");options.firstFrame=integer("first-frame");
    if(options.fpsBase<=0||options.fps<=0){error<<"Invalid frame rate.\n";return 2;}
    const double last=std::ceil(double(scene.durationMs)*options.fps/(1000.*options.fpsBase));
    if(last>2147483647.||last<0){error<<"Invalid scene duration.\n";return 2;}
    options.endFrame=parser.isSet("end-frame")?integer("end-frame"):int(last);
    if(!valid){error<<"Numeric options must be integers.\n";return 2;}
    const auto result=exportSceneFrames(scene,args[1],options,[&](int done,int total){if(done%25==0||done==total){log<<done<<" / "<<total<<" frames\n";log.flush();}return true;});
    if(!result.success){error<<result.error<<"\n";return 1;}log<<"Saved "<<result.written<<" frames to "<<QDir(args[1]).absolutePath()<<"\n";return 0;
}
