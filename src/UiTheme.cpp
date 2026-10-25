#include "UiTheme.h"
#include "UiControls.h"
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

QString currentUiTheme(){
    if(!qApp->property("cuebaseTheme").toString().isEmpty())return qApp->property("cuebaseTheme").toString();
    QSettings settings;
    if(settings.value("appearance/themeRevision",0).toInt()<2){settings.setValue("appearance/theme","Studio Blue");settings.setValue("appearance/themeRevision",2);}
    return settings.value("appearance/theme","Studio Blue").toString();
}

void applyUiTheme(const QString &requested,bool persist){
    const QString name=QStringList{"Studio Blue","Current","Darker","Light"}.contains(requested)?requested:"Studio Blue";
    const bool light=name=="Light",dark=name=="Darker",blue=name=="Studio Blue";
    QPalette p;
    p.setColor(QPalette::Window,QColor(blue?"#111b24":light?"#f0f0f2":dark?"#1d1d1f":"#303030"));
    p.setColor(QPalette::Base,QColor(blue?"#202f3b":light?"#ffffff":dark?"#29292c":"#454545"));
    p.setColor(QPalette::AlternateBase,QColor(blue?"#172632":light?"#e7e7ea":dark?"#242426":"#383838"));
    p.setColor(QPalette::Button,QColor(blue?"#283641":light?"#e0e0e4":dark?"#333337":"#414141"));
    const QColor text(light?"#28282d":"#dddddf"),muted(light?"#565862":"#b8bcc6");
    for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText,QPalette::ToolTipText})p.setColor(role,text);
    p.setColor(QPalette::PlaceholderText,muted);
    p.setColor(QPalette::ToolTipBase,p.color(QPalette::Base));
    p.setColor(QPalette::Mid,QColor(blue?"#344b5c":light?"#c3c3ca":dark?"#414146":"#555555"));
    p.setColor(QPalette::Light,QColor(light?"#fafafa":"#727278"));
    p.setColor(QPalette::Dark,QColor(light?"#d2d2d8":"#18181a"));
    p.setColor(QPalette::Highlight,QColor(blue?"#096fbd":light?"#c8def3":"#2563eb"));
    p.setColor(QPalette::HighlightedText,light?text:QColor("#ffffff"));
    p.setColor(QPalette::Link,QColor(light?"#256398":"#83b6e7"));
    for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText})p.setColor(QPalette::Disabled,role,QColor(light?"#6b6f79":"#8b97a3"));
    qApp->setProperty("cuebaseTheme",name);qApp->setPalette(p);
    QFont font=qApp->font();font.setPixelSize(10);qApp->setFont(font);
    qApp->setStyleSheet(R"(
QWidget { font-size:10px; color:palette(window-text); background:palette(window); }
QLabel,QCheckBox,QRadioButton { background:transparent; }
QFrame#panel,QFrame#editorFg,QFrame#editorBg,QFrame#qcard,QFrame#tlBox { border:1px solid palette(mid); border-radius:6px; }
QFrame#header { background:palette(alternate-base); border-bottom:1px solid palette(mid); }
QMenuBar,QMenuBar::item { background:transparent; padding:3px 8px; }
QMenuBar::item:selected { background:palette(button); border-radius:3px; }
QToolButton#windowClose:hover { background:#b63b48; color:white; }
QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,QPlainTextEdit,QTextEdit { background:palette(base); color:palette(text); border:1px solid palette(mid); border-radius:3px; padding:1px 4px; selection-background-color:palette(highlight); selection-color:palette(highlighted-text); }
QLineEdit:disabled,QComboBox:disabled,QSpinBox:disabled,QDoubleSpinBox:disabled { color:palette(text); background:palette(alternate-base); }
QPushButton,QToolButton { background:palette(button); color:palette(button-text); border:0; border-radius:3px; padding:3px 5px; }
QPushButton:hover,QToolButton:hover { background:palette(mid); }
QPushButton:checked,QToolButton:checked { background:palette(highlight); }
QPushButton:disabled,QToolButton:disabled { color:palette(button-text); }
QToolButton[sectionHeader="true"] { text-align:left; padding:3px; min-height:16px; }
QToolButton[sectionHeader="true"]:checked { background:palette(button); }
QToolButton::menu-indicator,QPushButton::menu-indicator { image:none; }
QComboBox::drop-down { border:0; width:20px; }
QComboBox QAbstractItemView { background:palette(base); color:palette(text); selection-background-color:palette(highlight); }
QListView#optionPopup { background:palette(window); border:1px solid palette(mid); border-radius:7px; padding:4px; outline:0; }
QListView#optionPopup::item { min-height:19px; padding:3px 8px; border-radius:5px; }
QListView#optionPopup::item:hover,QListView#optionPopup::item:selected { background:palette(highlight); color:palette(highlighted-text); }
QListWidget,QTreeWidget,QTextBrowser { background:palette(window); border:0; }
QListWidget::item,QTreeWidget::item { padding:2px; }
QListWidget::item:selected,QTreeWidget::item:selected { background:palette(highlight); color:palette(highlighted-text); }
QHeaderView::section { background:palette(button); color:palette(text); border:0; padding:3px; }
QMenu { background:palette(window); border:1px solid palette(mid); border-radius:7px; padding:4px; }
QMenu::item { padding:4px 22px 4px 10px; border-radius:5px; }
QMenu::item:selected { background:palette(highlight); color:palette(highlighted-text); }
QMenu::separator { height:1px; margin:4px; background:palette(mid); }
QScrollBar:vertical { width:4px; margin:0; background:transparent; }
QScrollBar:horizontal { height:4px; margin:0; background:transparent; }
QScrollBar::handle { background:palette(mid); border-radius:2px; min-width:20px; min-height:20px; }
QScrollBar::handle:hover { background:palette(light); }
QScrollBar::add-line,QScrollBar::sub-line { width:0; height:0; }
QScrollBar::add-page,QScrollBar::sub-page { background:transparent; }
QSplitter::handle { background:palette(dark); }
QGroupBox { border:1px solid palette(mid); border-radius:4px; margin-top:10px; padding:7px; }
QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 3px; }
QToolTip { background:palette(tool-tip-base); color:palette(tool-tip-text); border:1px solid palette(mid); padding:5px; }
QCheckBox { spacing:4px; }
)");
    if(blue)qApp->setStyleSheet(qApp->styleSheet()+QStringLiteral("QFrame#header{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #091a28,stop:1 #064565);border-bottom:1px solid #22516c;}QToolButton[sectionHeader=\"true\"]{background:#1d2c38;}QToolButton[sectionHeader=\"true\"]:checked{background:#1d2c38;}"));
    if(persist)QSettings().setValue("appearance/theme",name);
    for(auto *widget:qApp->allWidgets())widget->update();
}

void showUiSettings(QWidget *parent){
    auto *dialog=roundedModal(parent,"Settings");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(560,340);
    auto *outer=new QVBoxLayout(dialog);outer->setContentsMargins(16,16,16,16);auto *heading=new QLabel("Settings");heading->setStyleSheet("font-size:14px;font-weight:600;");outer->addWidget(heading);
    auto *row=new QHBoxLayout;auto *categories=new QListWidget;categories->setObjectName("settingsSections");categories->addItems({"General","Theme","Timeline"});categories->setFixedWidth(125);row->addWidget(categories);
    auto *pages=new QStackedWidget;auto *general=new QWidget;auto *generalLayout=new QVBoxLayout(general);auto *intro=new QLabel("Workspace appearance is saved on this computer.\nBroadcast content and exported graphics keep their own colors.");intro->setWordWrap(true);generalLayout->addWidget(intro);generalLayout->addStretch();pages->addWidget(general);
    auto *appearance=new QWidget;auto *form=new QFormLayout(appearance);form->setContentsMargins(18,4,4,4);auto *themes=new QComboBox;themes->setObjectName("applicationTheme");themes->addItems({"Studio Blue","Current","Darker","Light"});themes->setCurrentText(currentUiTheme());form->addRow("Theme",themes);auto *hint=new QLabel("Changes apply immediately to both workspaces.");hint->setWordWrap(true);form->addRow(hint);pages->addWidget(appearance);row->addWidget(pages,1);outer->addLayout(row,1);
    auto *timeline=new QWidget;auto *timelineForm=new QFormLayout(timeline);timelineForm->setContentsMargins(18,4,4,4);
    auto preference=[&](const QString &label,const QString &key,const QString &name){auto *check=new QCheckBox(label);check->setObjectName(name);check->setChecked(QSettings().value(key,true).toBool());timelineForm->addRow(check);QObject::connect(check,&QCheckBox::toggled,dialog,[key](bool enabled){QSettings().setValue(key,enabled);for(auto *widget:qApp->allWidgets())if(widget->inherits("TimelineWidget"))QMetaObject::invokeMethod(widget,"reloadPreferences",Qt::DirectConnection);});};
    preference("Keep the playhead centered","studio/fixedPlayhead","timelineFixedPlayhead");
    preference("Show video thumbnails","studio/timelineThumbnails","timelineThumbnails");
    preference("Show audio waveforms","studio/timelineWaveforms","timelineWaveforms");
    auto *timelineHint=new QLabel("Previews follow the source section used by each clip, including its trim.\nPan to inspect the timeline; press F to follow the broadcast again.");timelineHint->setWordWrap(true);timelineForm->addRow(timelineHint);pages->addWidget(timeline);
    QObject::connect(categories,&QListWidget::currentRowChanged,pages,&QStackedWidget::setCurrentIndex);QObject::connect(themes,&QComboBox::currentTextChanged,dialog,[](const QString &name){applyUiTheme(name);});categories->setCurrentRow(0);
    auto *close=new QPushButton("Done");close->setAutoDefault(false);QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::accept);outer->addWidget(close,0,Qt::AlignRight);dialog->open();
}
