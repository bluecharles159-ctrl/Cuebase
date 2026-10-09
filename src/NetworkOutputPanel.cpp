#include "NetworkOutputPanel.h"
#include "NetworkOutput.h"
#include "Data.h"
#include "Playout.h"
#include "UiControls.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QSettings>
#include <QUrl>

NetworkOutputPanel::NetworkOutputPanel(NetworkOutput *output,Library *library,Playout *playout,QWidget *parent):QWidget(parent){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,6,0,6);auto *title=new QLabel("NETWORK OUTPUT");title->setStyleSheet("font-size:10px;color:#93a8c0;font-weight:600;");layout->addWidget(title);
    auto *status=new QLabel(output->status());status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
    auto *button=new QPushButton("Configure / start stream");button->setIcon(uiIcon("play"));button->setToolTip("Send the clean program picture and mixed audio to an RTMP/RTMPS or SRT destination");layout->addWidget(button);
    auto settings=std::make_shared<StreamSettings>();QSettings saved;settings->executable=saved.value("stream/encoder",NetworkOutput::findEncoder()).toString();settings->videoKbps=saved.value("stream/videoKbps",6000).toInt();settings->audioKbps=saved.value("stream/audioKbps",160).toInt();
    connect(output,&NetworkOutput::statusChanged,this,[=](const QString &text,bool live){status->setText(text);status->setStyleSheet(live?"color:#63d4a4;":"color:#a5b3c6;");button->setText(output->active()?"Stop network stream":"Configure / start stream");button->setIcon(uiIcon(output->active()?"pause":"play"));});
    connect(playout,&Playout::stateChanged,this,[=]{if(!playout->isLive()&&output->active())output->stop();});
    connect(button,&QPushButton::clicked,this,[=,this]{
        if(output->active()){output->stop();return;}
        auto *dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("Live network output");dialog->setMinimumWidth(480);
        auto *box=new QVBoxLayout(dialog);auto *form=new QFormLayout;box->addLayout(form);
        auto *endpoint=new QLineEdit(settings->endpoint);endpoint->setPlaceholderText("rtmp://server/live or srt://host:port?mode=caller");endpoint->setToolTip("The broadcaster's RTMP/RTMPS or SRT destination. Connection credentials remain in memory for this session.");form->addRow("Destination",endpoint);
        auto *key=new QLineEdit;key->setEchoMode(QLineEdit::Password);key->setToolTip("Optional RTMP stream key, appended to the destination. For SRT, include connection options in the destination URL.");form->addRow("RTMP stream key",key);
        auto *encoder=new QLineEdit(settings->executable);auto *browse=new QPushButton("Choose FFmpeg…");form->addRow("Encoder",encoder);form->addRow(browse);
        connect(browse,&QPushButton::clicked,dialog,[=]{auto file=QFileDialog::getOpenFileName(dialog,"FFmpeg executable",encoder->text(),"Executable (*.exe);;All files (*)");if(!file.isEmpty())encoder->setText(file);});
        auto *video=new FrameInput,*audio=new FrameInput;video->setRange(100,100000);video->setValue(settings->videoKbps);video->setSuffix(" kbps");audio->setRange(32,512);audio->setValue(settings->audioKbps);audio->setSuffix(" kbps");form->addRow("H.264 bitrate",video);form->addRow("AAC bitrate",audio);
        auto *format=new QLabel(QString("%1 × %2 · %3 fps · stereo 48 kHz").arg(library->display.resX()).arg(library->display.resY()).arg(library->display.fps/library->display.fpsBase,0,'g',6));format->setToolTip("Resolution and frame rate follow Display Settings. Changes apply on the next connection.");form->addRow("Output",format);
        auto *error=new QLabel;error->setWordWrap(true);error->setStyleSheet("color:#f4b184;");box->addWidget(error);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText("Start stream");box->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,dialog,[=]{
            if(!playout->isLive()){error->setText("Start playout first so there is a program on air.");return;}
            StreamSettings next=*settings;next.executable=encoder->text().trimmed();next.endpoint=endpoint->text().trimmed();if(!key->text().isEmpty()&&QUrl(next.endpoint).scheme().startsWith("rtmp")){while(next.endpoint.endsWith('/'))next.endpoint.chop(1);next.endpoint+="/"+key->text();}
            next.size={library->display.resX(),library->display.resY()};next.fps=library->display.fps/library->display.fpsBase;next.videoKbps=video->value();next.audioKbps=audio->value();
            auto invalid=NetworkOutput::validate(next);if(!invalid.isEmpty()){error->setText(invalid);return;}
            *settings=next;QSettings save;save.setValue("stream/encoder",next.executable);save.setValue("stream/videoKbps",next.videoKbps);save.setValue("stream/audioKbps",next.audioKbps);
            if(output->start(next))dialog->accept();else error->setText(output->status());
        });dialog->open();
    });
}
