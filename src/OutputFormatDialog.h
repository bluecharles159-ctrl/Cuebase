#pragma once
#include "Data.h"
#include "UiControls.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

inline QDialog *createOutputFormatDialog(Library *library,QWidget *parent){
    auto *dialog=roundedModal(parent,"Channel output format");dialog->setObjectName("outputFormatDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);auto *layout=new QVBoxLayout(dialog);
    auto *description=new QLabel("Sets the channel's final picture, Build preview and exports. Changing the work camera never changes this output.");description->setWordWrap(true);layout->addWidget(description);
    auto *form=new QFormLayout;form->setContentsMargins(12,8,12,8);layout->addLayout(form);
    auto *preset=new QComboBox;preset->setObjectName("outputFormatPreset");
    preset->addItem("HD · 16:9",QSize(1920,1080));preset->addItem("UHD · 16:9",QSize(3840,2160));
    preset->addItem("Portrait · 9:16",QSize(1080,1920));preset->addItem("Square · 1:1",QSize(1080,1080));
    preset->addItem("Classic · 4:3",QSize(1440,1080));preset->addItem("Custom",QSize());form->addRow("Preset",preset);
    auto *width=new FrameInput,*height=new FrameInput;width->setObjectName("outputFormatWidth");height->setObjectName("outputFormatHeight");
    for(auto *field:{width,height}){field->setRange(16,8192);field->setSuffix(" px");}width->setValue(library->display.resX());height->setValue(library->display.resY());form->addRow("Width",width);form->addRow("Height",height);
    auto *pixelX=new ValueInput,*pixelY=new ValueInput;pixelX->setObjectName("outputPixelAspectX");pixelY->setObjectName("outputPixelAspectY");
    for(auto *field:{pixelX,pixelY}){field->setRange(.1,10);field->setDecimals(3);field->setSingleStep(.1);}pixelX->setValue(library->display.pixAspX);pixelY->setValue(library->display.pixAspY);form->addRow("Pixel aspect X",pixelX);form->addRow("Pixel aspect Y",pixelY);
    auto updatePreset=[=]{QSignalBlocker block(preset);const int found=preset->findData(QSize(width->value(),height->value()));preset->setCurrentIndex(found<0?preset->count()-1:found);};updatePreset();
    QObject::connect(width,&QSpinBox::valueChanged,dialog,updatePreset);QObject::connect(height,&QSpinBox::valueChanged,dialog,updatePreset);
    QObject::connect(preset,&QComboBox::activated,dialog,[=]{const auto size=preset->currentData().toSize();if(!size.isValid())return;QSignalBlocker a(width),b(height);width->setValue(size.width());height->setValue(size.height());pixelX->setValue(1);pixelY->setValue(1);});
    auto *error=new QLabel;error->setObjectName("outputFormatError");error->setWordWrap(true);error->setStyleSheet("color:#f87171;");error->hide();layout->addWidget(error);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog,[=]{
        const auto before=library->display;library->display.resolution=QString("%1x%2").arg(width->value()).arg(height->value());library->display.pixAspX=pixelX->value();library->display.pixAspY=pixelY->value();
        QString reason;if(!library->saveChecked(&reason)){library->display=before;error->setText("Could not save the output format. "+reason);error->show();return;}
        emit library->displayChanged();dialog->accept();
    });
    dialog->resize(400,350);return dialog;
}
