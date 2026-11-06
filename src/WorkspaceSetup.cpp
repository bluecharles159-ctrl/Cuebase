#include "WorkspaceSetup.h"
#include "Data.h"
#include "UiControls.h"
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QImageReader>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

bool WorkspaceSetup::editingOnly(const Library &lib) {
    return lib.production.value("workspaceProfile").toObject().value("mode").toString()=="editing";
}

QDialog *WorkspaceSetup::dialog(QWidget *parent,Library *lib) {
    auto *dialog=roundedModal(parent,"Workspace setup");
    dialog->setObjectName("workspaceSetup");
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumWidth(495);
    auto *root=new QVBoxLayout(dialog);root->setContentsMargins(24,20,24,20);root->setSpacing(13);
    auto *title=new QLabel("Set up your workspace");
    QFont font=title->font();font.setPointSize(font.pointSize()+5);font.setBold(true);title->setFont(font);
    root->addWidget(title);
    auto *choices=new QHBoxLayout;
    auto *group=new QButtonGroup(dialog);
    auto option=[&](const QString &label,const QString &icon,const QString &name) {
        auto *b=new QPushButton(uiIcon(icon),label);b->setObjectName(name);b->setCheckable(true);
        b->setMinimumHeight(42);group->addButton(b);choices->addWidget(b,1);return b;
    };
    auto *studio=option("TV production","broadcast","setupStudio"),*editing=option("Editing","build","setupEditing");
    root->addLayout(choices);
    auto *description=new QLabel;description->setWordWrap(true);root->addWidget(description);
    auto *details=new QWidget;auto *fields=new QFormLayout(details);fields->setContentsMargins(0,5,0,5);fields->setSpacing(8);
    auto *company=new QLineEdit(lib->production.value("workspaceProfile").toObject().value("production").toString());
    company->setPlaceholderText("Production or studio name (optional)");company->setMaxLength(160);fields->addRow("Production",company);
    auto *name=new QLineEdit(lib->display.channel);name->setObjectName("setupChannelName");name->setMaxLength(160);fields->addRow("Channel name",name);
    auto *logoRow=new QWidget;auto *row=new QHBoxLayout(logoRow);row->setContentsMargins(0,0,0,0);
    auto *logo=new QLineEdit(lib->display.channelLogo);logo->setPlaceholderText("Optional");logo->setObjectName("setupLogo");
    auto *browse=new QPushButton(uiIcon("image"),"Browse…");row->addWidget(logo,1);row->addWidget(browse);fields->addRow("Channel logo",logoRow);
    QObject::connect(browse,&QPushButton::clicked,dialog,[=]{
        auto path=QFileDialog::getOpenFileName(dialog,"Channel logo",{},"Images (*.png *.jpg *.jpeg *.webp *.svg)");
        if(!path.isEmpty())logo->setText(path);
    });
    auto *format=new QComboBox;format->setObjectName("setupFormat");
    format->addItems({"1280x720","1920x1080","3840x2160","1080x1920"});
    if(format->findText(lib->display.resolution)<0)format->addItem(lib->display.resolution);
    format->setCurrentText(lib->display.resolution);fields->addRow("Output size",format);
    auto *fps=new QComboBox;fps->setObjectName("setupFrameRate");
    for(int rate:{24,25,30,50,60})fps->addItem(QString::number(rate)+" fps",rate);
    if(lib->display.fpsBase!=1. || fps->findData(lib->display.fps)<0){
        fps->addItem(QString::number(lib->display.fps/lib->display.fpsBase,'g',6)+" fps (current)",-1);fps->setCurrentIndex(fps->count()-1);
    }else fps->setCurrentIndex(fps->findData(lib->display.fps));
    fields->addRow("Frame rate",fps);
    auto *destination=new QLabel("When you’re ready to broadcast, enter your destination and stream key in Studio’s Network Output controls.");
    destination->setWordWrap(true);fields->addRow(destination);root->addWidget(details);
    auto *notice=new QLabel("This workspace is saved on this computer.");root->addWidget(notice);
    auto *error=new QLabel;error->setObjectName("setupError");error->setWordWrap(true);error->hide();root->addWidget(error);
    auto *actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);root->addWidget(actions);
    auto *save=actions->button(QDialogButtonBox::Save);save->setObjectName("saveWorkspaceSetup");
    auto update=[=] {
        details->setVisible(studio->isChecked());
        description->setText(studio->isChecked()?"Prepare programs, plan a channel and send the finished picture to air.":
            "Create graphics, animate scenes and edit media. Studio playback is available for local preview.");
        save->setText(studio->isChecked()?"Open Studio":"Open Build");
        dialog->adjustSize();
    };
    QObject::connect(studio,&QPushButton::toggled,dialog,update);
    (editingOnly(*lib)?editing:studio)->setChecked(true);update();
    QObject::connect(actions,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    QObject::connect(actions,&QDialogButtonBox::accepted,dialog,[=]{
        const bool production=studio->isChecked();
        auto fail=[&](QString msg){error->setText(msg);error->show();};
        if(production && name->text().trimmed().isEmpty()){fail("Enter a channel name.");name->setFocus();return;}
        const QString file=logo->text().trimmed();
        if(production && file!=lib->display.channelLogo && !file.isEmpty() && !QImageReader(file).canRead()){
            fail("Choose a readable channel logo.");return;
        }
        const auto oldData=lib->production;const auto oldDisplay=lib->display;
        auto profile=oldData.value("workspaceProfile").toObject();
        profile["mode"]=production?"studio":"editing";
        if(production){
            profile["production"]=company->text().trimmed();lib->display.channel=name->text().trimmed();
            if(lib->display.channelLogo!=file)lib->display.channelLogoElement.clear();
            lib->display.channelLogo=file;lib->display.resolution=format->currentText();
            if(fps->currentData().toInt()>0){lib->display.fps=fps->currentData().toInt();lib->display.fpsBase=1.;}
        }
        lib->production["workspaceProfile"]=profile;QString reason;
        if(!lib->saveChecked(&reason)){lib->production=oldData;lib->display=oldDisplay;fail(reason);return;}
        emit lib->productionChanged();emit lib->displayChanged();dialog->accept();
    });
    return dialog;
}
