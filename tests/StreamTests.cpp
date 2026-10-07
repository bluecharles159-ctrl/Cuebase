#include <QtTest>
#include <QDataStream>
#include <cmath>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QUdpSocket>
#include "NetworkOutput.h"
#include "Playout.h"
#include "ViewportWidget.h"

class StreamTests:public QObject{
    Q_OBJECT
private slots:
    void cleanProgram(){
        Library library;library.display.watermarkOn=false;library.display.safeArea=false;
        ProgramItem program;program.id="test";program.title="Test program";library.programs={program};
        ElementDesign design=ElementDesign::defaultScene();DObject rectangle;rectangle.id="graphic";rectangle.type="Shape";rectangle.w=4;rectangle.h=2;rectangle.base.rot={90,0,0};rectangle.mat.color=Qt::red;rectangle.mat.shadeless=true;design.objects.append(rectangle);
        Element element;element.id="graphic";element.type="Custom";element.live=true;element.design=design.toJson();library.elements.append(element);
        Playout play(&library);ViewportWidget monitor(&library,&play);const QSize size(640,360);
        QImage black(size,QImage::Format_ARGB32);black.fill(Qt::black);QCOMPARE(monitor.programFrame(size),black);
        play.start();play.togglePause();const auto clean=monitor.programFrame(size);QVERIFY(clean!=black);QVERIFY(qRed(clean.pixel(320,180))>200);QVERIFY(qGreen(clean.pixel(320,180))<30);
        library.display.safeArea=true;QCOMPARE(monitor.programFrame(size),clean);
        library.elements.first().live=false;QVERIFY(monitor.programFrame(size)!=clean);
        play.stop();QCOMPARE(monitor.programFrame(size),black);
    }
    void validation(){StreamSettings s;s.executable=NetworkOutput::findEncoder();QVERIFY(!s.executable.isEmpty());s.endpoint="file:///tmp/output";QVERIFY(!NetworkOutput::validate(s).isEmpty());s.endpoint="rtmp://127.0.0.1/live/test";QVERIFY(NetworkOutput::validate(s).isEmpty());s.size={321,180};QVERIFY(!NetworkOutput::validate(s).isEmpty());}
    void loopback_data(){QTest::addColumn<QString>("protocol");QTest::newRow("RTMP")<<QString("rtmp");QTest::newRow("SRT")<<QString("srt");}
    void loopback(){
        QFETCH(QString,protocol);QTemporaryDir temp;QVERIFY(temp.isValid());const QString ffmpeg=NetworkOutput::findEncoder();QVERIFY(!ffmpeg.isEmpty());
        QFile tone(temp.filePath("tone.wav"));QVERIFY(tone.open(QIODevice::WriteOnly));
        QDataStream wav(&tone);wav.setByteOrder(QDataStream::LittleEndian);
        const quint32 bytes=48000*8*4;wav.writeRawData("RIFF",4);wav<<quint32(36+bytes);wav.writeRawData("WAVEfmt ",8);wav<<quint32(16)<<quint16(1)<<quint16(2)<<quint32(48000)<<quint32(192000)<<quint16(4)<<quint16(16);wav.writeRawData("data",4);wav<<bytes;
        for(int i=0;i<48000*8;++i){qint16 sample=qint16(6000*std::sin(i*2*3.141592653589793*440/48000));wav<<sample<<sample;}tone.close();
        quint16 port=0;{if(protocol=="rtmp"){QTcpServer reserve;QVERIFY(reserve.listen(QHostAddress::LocalHost,0));port=reserve.serverPort();}else{QUdpSocket reserve;QVERIFY(reserve.bind(QHostAddress(QHostAddress::LocalHost),quint16(0)));port=reserve.localPort();}}
        const QString target=protocol=="rtmp"?QString("rtmp://127.0.0.1:%1/live/test").arg(port):QString("srt://127.0.0.1:%1?mode=caller&latency=120000").arg(port);
        const QString received=temp.filePath(protocol=="rtmp"?"received.flv":"received.ts");
        QProcess server;QByteArray serverLog;
        if(protocol=="rtmp"){
            const QString executable=QCoreApplication::applicationDirPath()+"/../tools/mediamtx/mediamtx.exe";QVERIFY2(QFileInfo::exists(executable),"Install the local MediaMTX test receiver in tools/mediamtx.");
            QFile config(temp.filePath("mediamtx.yml"));QVERIFY(config.open(QIODevice::WriteOnly));config.write(QString("logLevel: info\nrtsp: no\nhls: no\nwebrtc: no\nmoq: no\nsrt: no\nrtmp: yes\nrtmpAddress: 127.0.0.1:%1\npaths:\n  all_others:\n").arg(port).toUtf8());config.close();
            server.setProcessChannelMode(QProcess::MergedChannels);server.start(executable,{config.fileName()});QVERIFY(server.waitForStarted(10000));QByteArray log;
            QTRY_VERIFY_WITH_TIMEOUT((log+=server.readAll()).contains("started with listener"),15000);
        }
        QStringList args{"-hide_banner","-loglevel","error"};if(protocol=="rtmp")args<<"-rtmp_live"<<"live";args<<"-i"<<(protocol=="rtmp"?target:QString("srt://127.0.0.1:%1?mode=listener&latency=120000").arg(port))<<"-t"<<"3"<<"-c"<<"copy"<<"-y"<<received;
        QProcess receiver;if(protocol!="rtmp"){receiver.start(ffmpeg,args);QVERIFY(receiver.waitForStarted(10000));QTest::qWait(1000);}
        NetworkOutput output;int frames=0;output.setFrameProvider([&](QSize size){QImage image(size,QImage::Format_ARGB32);image.fill(QColor(30+(frames++%100),90,190));return image;});
        QMediaPlayer player;player.setLoops(QMediaPlayer::Infinite);output.attachPlayer(&player);player.setSource(QUrl::fromLocalFile(temp.filePath("tone.wav")));player.play();
        StreamSettings settings;settings.executable=ffmpeg;settings.endpoint=target;settings.size={320,180};settings.fps=25;settings.videoKbps=500;settings.reconnects=0;
        QVERIFY(output.start(settings));QTRY_VERIFY_WITH_TIMEOUT(output.streaming()||!output.active(),20000);QVERIFY2(output.streaming(),qPrintable(output.status()));
        if(protocol=="rtmp"){QTRY_VERIFY2_WITH_TIMEOUT((serverLog+=server.readAll()).contains("is publishing"),serverLog.constData(),15000);receiver.start(ffmpeg,args);QVERIFY(receiver.waitForStarted(10000));}
        QTRY_VERIFY_WITH_TIMEOUT(receiver.state()==QProcess::NotRunning,20000);const auto outputStatus=output.status();output.stop();player.stop();if(server.state()!=QProcess::NotRunning){server.kill();server.waitForFinished(5000);}
        const auto errors=receiver.readAllStandardError()+outputStatus.toUtf8()+serverLog+server.readAll();QVERIFY2(QFileInfo(received).size()>1000,errors.constData());
        const QString ffprobe=QFileInfo(ffmpeg).dir().filePath("ffprobe.exe");QProcess probe;probe.start(ffprobe,{"-v","error","-show_streams","-of","json",received});QVERIFY(probe.waitForFinished(10000));auto streams=QJsonDocument::fromJson(probe.readAllStandardOutput()).object()["streams"].toArray();bool video=false,audio=false;
        for(const auto &value:streams){auto stream=value.toObject();if(stream["codec_type"]=="video"){video=true;QCOMPARE(stream["codec_name"].toString(),QString("h264"));QCOMPARE(stream["width"].toInt(),320);QCOMPARE(stream["height"].toInt(),180);}if(stream["codec_type"]=="audio"){audio=true;QCOMPARE(stream["codec_name"].toString(),QString("aac"));}}
        QVERIFY(video);QVERIFY(audio);QProcess decode;decode.start(ffmpeg,{"-hide_banner","-loglevel","error","-i",received,"-map","0:a:0","-f","s16le","-ac","1","-ar","48000","pipe:1"});QVERIFY(decode.waitForFinished(10000));auto samples=decode.readAllStandardOutput();QVERIFY(samples.size()>48000);qint64 energy=0;for(int i=0;i+1<samples.size();i+=2){qint16 sample=qint16(quint8(samples[i])|(quint16(quint8(samples[i+1]))<<8));energy+=qAbs(int(sample));}QVERIFY2(energy/(samples.size()/2)>100,"Stream audio was silent");
        QDir().mkpath("artifacts");QFile::remove(QString("artifacts/loopback-%1.%2").arg(protocol,protocol=="rtmp"?"flv":"ts"));QVERIFY(QFile::copy(received,QString("artifacts/loopback-%1.%2").arg(protocol,protocol=="rtmp"?"flv":"ts"))||QFileInfo::exists(QString("artifacts/loopback-%1.%2").arg(protocol,protocol=="rtmp"?"flv":"ts")));
    }
};
QTEST_MAIN(StreamTests)
#include "StreamTests.moc"
