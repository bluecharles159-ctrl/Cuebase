#include "DesignAssets.h"
#include "BuildModel.h"
#include "UiControls.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <memory>

QDialog *createDesignMediaDialog(BuildModel *model,QWidget *parent){
    auto *dialog=new QDialog(parent);dialog->setObjectName("compositionMediaDialog");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("Composition media");dialog->resize(760,440);
    auto draft=std::make_shared<ElementDesign>(model->design);const auto original=model->design.toJson();
    auto *layout=new QVBoxLayout(dialog);layout->setContentsMargins(16,16,16,16);layout->setSpacing(10);
    auto *intro=new QLabel("Relink a source once to update every object using it. Changes apply together and can be undone.");intro->setWordWrap(true);layout->addWidget(intro);
    auto *filters=new QHBoxLayout;auto *search=new QLineEdit;search->setPlaceholderText("Find media or objects…");search->setObjectName("mediaSearch");auto *missingOnly=new QCheckBox("Missing only");filters->addWidget(search,1);filters->addWidget(missingOnly);layout->addLayout(filters);
    auto *tree=new QTreeWidget;tree->setObjectName("compositionMediaList");tree->setHeaderLabels({"Media","Used by","Status","Location"});tree->setRootIsDecorated(false);tree->setAlternatingRowColors(true);tree->setSelectionMode(QAbstractItemView::SingleSelection);tree->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);tree->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);tree->header()->setSectionResizeMode(2,QHeaderView::ResizeToContents);tree->header()->setStretchLastSection(true);layout->addWidget(tree,1);
    auto *status=new QLabel;status->setObjectName("mediaStatus");status->setWordWrap(true);layout->addWidget(status);
    auto *relink=new QPushButton(uiIcon("folder"),"Relink selected media…");relink->setObjectName("relinkCompositionMedia");relink->setEnabled(false);layout->addWidget(relink);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Apply)->setEnabled(false);layout->addWidget(buttons);
    auto filter=[=]{for(int i=0;i<tree->topLevelItemCount();++i){auto *item=tree->topLevelItem(i);const bool matches=(item->text(0)+" "+item->text(1)+" "+item->text(3)).contains(search->text(),Qt::CaseInsensitive);item->setHidden(!matches||(missingOnly->isChecked()&&!item->data(0,Qt::UserRole+1).toBool()));}if(tree->currentItem()&&tree->currentItem()->isHidden())tree->setCurrentItem(nullptr);relink->setEnabled(tree->currentItem()!=nullptr);};
    auto refresh=[=]{
        const auto selected=tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toString():QString();tree->clear();QMap<QString,QStringList> uses;
        for(const auto &asset:designAssets(*draft))uses[asset.path].append(asset.objectName.isEmpty()?asset.objectId:asset.objectName);
        int missing=0;for(auto it=uses.cbegin();it!=uses.cend();++it){const QFileInfo file(it.key());const bool absent=!file.isFile();missing+=absent;auto *item=new QTreeWidgetItem(tree,{file.fileName(),it.value().join(", "),absent?"Missing":"Available",it.key()});item->setData(0,Qt::UserRole,it.key());item->setData(0,Qt::UserRole+1,absent);item->setToolTip(3,it.key());if(absent)item->setForeground(2,QColor("#f0a060"));if(it.key()==selected)tree->setCurrentItem(item);}
        status->setText(uses.isEmpty()?"This composition has no linked media.":QString("%1 sources · %2 missing").arg(uses.size()).arg(missing));buttons->button(QDialogButtonBox::Apply)->setEnabled(draft->toJson()!=original);filter();
    };
    QObject::connect(search,&QLineEdit::textChanged,dialog,[=]{filter();});QObject::connect(missingOnly,&QCheckBox::toggled,dialog,[=]{filter();});
    QObject::connect(tree,&QTreeWidget::currentItemChanged,dialog,[=](QTreeWidgetItem *item){relink->setEnabled(item);});
    QObject::connect(relink,&QPushButton::clicked,dialog,[=]{const auto *item=tree->currentItem();if(!item)return;const auto old=item->data(0,Qt::UserRole).toString();const auto file=QFileDialog::getOpenFileName(dialog,"Relink media",QFileInfo(old).absolutePath(),"Media files (*.*)",nullptr,QFileDialog::DontUseNativeDialog);if(file.isEmpty())return;QString error;if(!relinkDesignAsset(*draft,old,file,&error)){status->setText(error);return;}refresh();});
    QObject::connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,dialog,[=]{if(model->design.toJson()!=original){status->setText("The composition changed while this window was open. Reopen Composition media before applying.");return;}if(draft->toJson()!=original){model->pushUndo();model->design=*draft;model->notify();}dialog->accept();});
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);refresh();return dialog;
}
