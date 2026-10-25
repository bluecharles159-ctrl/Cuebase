#include "TextureLibrary.h"
#include "BuildModel.h"
#include "Data.h"
#include "DesignAssets.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDrag>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
class TextureList : public QListWidget {
protected:
    void startDrag(Qt::DropActions) override {
        if(!currentItem())return;
        auto *mime=new QMimeData;mime->setData(TextureLibrary::mimeType,currentItem()->data(Qt::UserRole).toString().toUtf8());
        auto *drag=new QDrag(this);drag->setMimeData(mime);drag->setPixmap(currentItem()->icon().pixmap(80,60));drag->exec(Qt::CopyAction);
    }
};
class TexturePanel : public QWidget {
    BuildModel *model;Library *library;
    TextureList *list;QLineEdit *search,*name,*collection;QComboBox *filter,*kind;QCheckBox *favorites,*favorite;
    QLabel *status,*selection;QPushButton *apply,*relink,*save,*exportButton,*remove;
    QJsonArray snapshot;QHash<QString,QIcon> thumbnails;
    QString current() const {return list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString();}
    void report(const QString &error){status->setText(error);status->setVisible(!error.isEmpty());}
    void details(){
        const auto item=TextureLibrary::entry(*library,current());const bool exists=!item.isEmpty();
        name->setText(item["name"].toString());collection->setText(item["collection"].toString());favorite->setChecked(item["favorite"].toBool());
        for(QWidget *widget:QList<QWidget*>{name,collection,favorite,save,exportButton,remove,relink})widget->setEnabled(exists);
        const auto file=TextureLibrary::file(item);relink->setText(!file.isEmpty()&&!QFileInfo::exists(file)?"Relink missing texture…":"Replace texture…");
        selection->setText(!exists?"Import a texture, or save the selected object's surface as a preset.":file.isEmpty()?"Surface preset · no external image":QFileInfo::exists(file)?"Ready · double-click to apply, or drag onto an object":"Missing texture · relink before applying");
        bool compatible=false;for(auto *object:model->selected())if(!object->locked&&TextureLibrary::compatible(*object))compatible=true;
        apply->setEnabled(exists&&compatible&&!model->editMode);
    }
    void refresh(const QString &selected={}){
        const auto id=selected.isEmpty()?current():selected;
        {QSignalBlocker block(list);list->clear();
            for(auto value:TextureLibrary::entries(*library)){
                const auto item=value.toObject();const auto itemId=item["id"].toString();
                if(favorites->isChecked()&&!item["favorite"].toBool())continue;
                if(filter->currentIndex()>0&&item["collection"].toString()!=filter->currentText())continue;
                if(kind->currentIndex()>0&&item["kind"].toString()!=kind->currentData().toString())continue;
                if(!(item["name"].toString()+" "+item["collection"].toString()).contains(search->text(),Qt::CaseInsensitive))continue;
                if(!thumbnails.contains(itemId))thumbnails[itemId]=QIcon(QPixmap::fromImage(TextureLibrary::thumbnail(item)));
                const auto path=TextureLibrary::file(item);const bool missing=!path.isEmpty()&&!QFileInfo::exists(path);
                auto *row=new QListWidgetItem(thumbnails[itemId],(item["favorite"].toBool()?QString::fromUtf8("★ "):QString())+item["name"].toString()+(missing?"\nMissing":""),list);
                row->setData(Qt::UserRole,itemId);row->setToolTip(item["kind"].toString()+" · "+item["collection"].toString()+"\n"+path);
                if(itemId==id)list->setCurrentItem(row);
            }
        }details();
    }
    void libraryChanged(){
        const auto next=TextureLibrary::entries(*library);if(snapshot==next)return;snapshot=next;thumbnails.clear();
        const auto before=filter->currentText();QStringList collections;
        for(auto value:next){const auto c=value.toObject()["collection"].toString();if(!collections.contains(c))collections.append(c);}collections.sort(Qt::CaseInsensitive);
        {QSignalBlocker block(filter);filter->clear();filter->addItem("All collections");filter->addItems(collections);filter->setCurrentIndex(qMax(0,filter->findText(before)));}
        refresh();
    }
public:
    TexturePanel(BuildModel *m,Library *lib,QWidget *parent):QWidget(parent),model(m),library(lib){
        setStyleSheet("QComboBox,QLineEdit{min-height:18px;padding:1px 3px;} QToolButton{border:0;text-align:left;padding:3px;} QToolButton:checked{background:palette(button);} QPushButton{padding:3px;border-radius:3px;} QLabel{font-size:10px;}");
        setObjectName("textureLibraryPanel");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(5);
        auto *header=new QToolButton;header->setText("TEXTURES && MATERIAL PRESETS");header->setCheckable(true);header->setChecked(true);header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);header->setArrowType(Qt::DownArrow);header->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);layout->addWidget(header);
        auto *body=new QWidget;auto *bodyLayout=new QVBoxLayout(body);bodyLayout->setContentsMargins(10,4,0,4);bodyLayout->setSpacing(5);layout->addWidget(body);
        connect(header,&QToolButton::toggled,this,[=](bool on){body->setVisible(on);header->setArrowType(on?Qt::DownArrow:Qt::RightArrow);});
        search=new QLineEdit;search->setObjectName("textureSearch");search->setPlaceholderText("Search textures or collections…");bodyLayout->addWidget(search);
        filter=new QComboBox;filter->setObjectName("textureCollectionFilter");filter->addItem("All collections");bodyLayout->addWidget(filter);
        auto *row=new QHBoxLayout;kind=new QComboBox;kind->addItem("All materials","");kind->addItem("Textures","Texture");kind->addItem("Presets","Preset");favorites=new QCheckBox("Favorites");favorites->setObjectName("textureFavoritesOnly");row->addWidget(kind);row->addWidget(favorites);bodyLayout->addLayout(row);
        list=new TextureList;list->setObjectName("textureItems");list->setViewMode(QListView::IconMode);list->setResizeMode(QListView::Adjust);list->setMovement(QListView::Static);list->setIconSize({96,72});list->setGridSize({112,110});list->setSpacing(3);list->setWordWrap(true);list->setDragEnabled(true);list->setMinimumHeight(180);list->setMaximumHeight(280);bodyLayout->addWidget(list);
        auto *actions=new QHBoxLayout;auto *import=new QPushButton("Import…");import->setObjectName("textureImport");auto *preset=new QPushButton("Save surface preset");preset->setObjectName("textureSavePreset");actions->addWidget(import);actions->addWidget(preset);bodyLayout->addLayout(actions);
        selection=new QLabel;selection->setWordWrap(true);bodyLayout->addWidget(selection);
        auto *form=new QFormLayout;form->setContentsMargins(0,0,0,0);name=new QLineEdit;name->setObjectName("textureName");collection=new QLineEdit;collection->setObjectName("textureCollection");favorite=new QCheckBox("Favorite");favorite->setObjectName("textureFavorite");form->addRow("Name",name);form->addRow("Collection",collection);form->addRow(favorite);bodyLayout->addLayout(form);
        save=new QPushButton("Save library details");save->setObjectName("textureSaveDetails");bodyLayout->addWidget(save);
        apply=new QPushButton("Apply to selected objects");apply->setObjectName("textureApply");bodyLayout->addWidget(apply);
        relink=new QPushButton;relink->setObjectName("textureRelink");bodyLayout->addWidget(relink);
        auto *bottom=new QHBoxLayout;exportButton=new QPushButton("Export preset…");exportButton->setObjectName("textureExport");remove=new QPushButton("Remove from library");remove->setToolTip("Keeps the image files used by existing scenes.");bottom->addWidget(exportButton);bottom->addWidget(remove);bodyLayout->addLayout(bottom);
        status=new QLabel;status->setWordWrap(true);status->setObjectName("textureStatus");bodyLayout->addWidget(status);
        auto applyCurrent=[this]{QString error;if(TextureLibrary::apply(*model,*library,current(),{},&error))report("Applied · view in Material or Final mode");else report(error);};
        connect(apply,&QPushButton::clicked,this,applyCurrent);connect(list,&QListWidget::itemDoubleClicked,this,[=]{applyCurrent();});
        connect(list,&QListWidget::currentItemChanged,this,[this]{details();});
        connect(search,&QLineEdit::textChanged,this,[this]{refresh();});connect(filter,&QComboBox::currentIndexChanged,this,[this]{refresh();});connect(kind,&QComboBox::currentIndexChanged,this,[this]{refresh();});connect(favorites,&QCheckBox::toggled,this,[this]{refresh();});
        connect(import,&QPushButton::clicked,this,[this]{
            const auto paths=QFileDialog::getOpenFileNames(this,"Import textures or material presets",{},"Images and materials (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff *.cuemat)");if(paths.isEmpty())return;
            QStringList failures;QString last;int count=0;for(const auto &path:paths){QString error;const auto id=TextureLibrary::importFile(*library,path,&error);if(id.isEmpty())failures.append(QFileInfo(path).fileName()+": "+error);else{last=id;++count;}}
            search->clear();favorites->setChecked(false);filter->setCurrentIndex(0);kind->setCurrentIndex(0);refresh(last);report(QString("Imported %1 item(s).").arg(count)+(failures.isEmpty()?QString():"\n"+failures.join('\n')));
        });
        connect(preset,&QPushButton::clicked,this,[this]{if(!model->active()){report("Select a text, shape or image first.");return;}QString error;const auto id=TextureLibrary::savePreset(*library,*model->active(),model->active()->name+" surface",&error);if(id.isEmpty()){report(error);return;}search->clear();favorites->setChecked(false);filter->setCurrentIndex(0);kind->setCurrentIndex(0);refresh(id);report("Surface preset saved. Rename it below.");});
        connect(save,&QPushButton::clicked,this,[this]{const auto id=current();QString error;const bool ok=TextureLibrary::update(*library,id,name->text(),collection->text(),favorite->isChecked(),&error);refresh(id);report(ok?"Library details saved.":error);});
        connect(relink,&QPushButton::clicked,this,[this]{const auto id=current(),old=TextureLibrary::file(TextureLibrary::entry(*library,id));const auto path=QFileDialog::getOpenFileName(this,"Relink texture",{},"Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)");if(path.isEmpty())return;QString error;
            if(!TextureLibrary::relink(*library,id,path,&error)){report(error);return;}
            if(!old.isEmpty()){auto design=model->design;const auto replacement=TextureLibrary::file(TextureLibrary::entry(*library,id));if(relinkDesignAsset(design,old,replacement,&error)){model->pushUndo();model->design=design;model->notify();}}
            refresh(id);report("Texture repaired in the library and matching objects in this scene.");
        });
        connect(exportButton,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getSaveFileName(this,"Export portable surface preset",name->text()+".cuemat","Cuebase material (*.cuemat)");if(path.isEmpty())return;QString error;report(TextureLibrary::exportEntry(*library,current(),path.endsWith(".cuemat",Qt::CaseInsensitive)?path:path+".cuemat",&error)?"Preset exported with its texture assets.":error);});
        connect(remove,&QPushButton::clicked,this,[this]{QString error;report(TextureLibrary::remove(*library,current(),&error)?"Removed from library. Existing scene files were kept.":error);});
        connect(library,&Library::productionChanged,this,[this]{libraryChanged();});connect(model,&BuildModel::selectionChanged,this,[this]{details();});connect(model,&BuildModel::modeChanged,this,[this]{details();});
        libraryChanged();refresh();report({});
    }
};
}
QWidget *TextureLibrary::createPanel(BuildModel *model,Library *library,QWidget *parent){return new TexturePanel(model,library,parent);}
