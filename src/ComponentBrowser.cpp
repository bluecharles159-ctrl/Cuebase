#include "ComponentLibrary.h"
#include "BuildModel.h"
#include "Data.h"
#include "UiControls.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>

QDialog *Components::createBrowser(BuildModel *model,Library *library,QWidget *parent){
    auto *dialog=roundedModal(parent,"Reusable objects");dialog->setWindowModality(Qt::NonModal);dialog->setObjectName("componentBrowser");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(740,700);
    auto *layout=new QVBoxLayout(dialog);layout->setContentsMargins(16,16,16,16);layout->setSpacing(8);
    auto *heading=new QLabel("Reusable objects");heading->setStyleSheet("font-size:16px;font-weight:600;background:transparent;");layout->addWidget(heading);
    auto *intro=new QLabel("Create once, reuse across scenes. Rename an object ::divider to register it. Text, image content and placement remain local to each instance.");intro->setWordWrap(true);layout->addWidget(intro);
    auto *filters=new QHBoxLayout;auto *search=new QLineEdit;search->setObjectName("componentSearch");search->setPlaceholderText("Search reusable objects…");auto *category=new QComboBox;category->setObjectName("componentCategoryFilter");auto *recent=new QCheckBox("Recently used first");recent->setChecked(true);filters->addWidget(search,1);filters->addWidget(category);filters->addWidget(recent);layout->addLayout(filters);
    auto *list=new QListWidget;list->setObjectName("componentItems");list->setViewMode(QListView::IconMode);list->setResizeMode(QListView::Adjust);list->setMovement(QListView::Static);list->setIconSize({128,80});list->setGridSize({158,125});list->setWordWrap(true);list->setMinimumHeight(200);layout->addWidget(list,1);
    auto *actions=new QHBoxLayout;auto *insertButton=new QPushButton(uiIcon("add"),"Insert linked instance");insertButton->setObjectName("componentInsert");auto *publishButton=new QPushButton("Publish selected source");publishButton->setObjectName("componentPublish");actions->addWidget(insertButton);actions->addWidget(publishButton);layout->addLayout(actions);
    auto *details=new QFormLayout;auto *name=new QLineEdit;name->setObjectName("componentName");auto *collection=new QLineEdit;collection->setObjectName("componentCategory");details->addRow("Library name",name);details->addRow("Category",collection);auto *save=new QPushButton("Save library details");save->setObjectName("componentSaveDetails");details->addRow(save);layout->addLayout(details);
    auto *state=new QLabel;state->setObjectName("componentLinkStatus");state->setWordWrap(true);layout->addWidget(state);
    auto *linksRow=new QHBoxLayout;QHash<QString,QCheckBox*> links;
    for(const auto &group:groups()){auto *check=new QCheckBox(group=="Cues"?"Animation cues":group);check->setObjectName("componentLink"+group);check->setChecked(true);links[group]=check;linksRow->addWidget(check);}layout->addLayout(linksRow);
    auto *saveLinks=new QPushButton("Save links for selected object");saveLinks->setObjectName("componentSaveLinks");layout->addWidget(saveLinks);
    auto *updateButton=new QPushButton("Review source changes for selected instances…");updateButton->setObjectName("componentReviewUpdate");layout->addWidget(updateButton);
    auto *status=new QLabel;status->setObjectName("componentStatus");status->setWordWrap(true);layout->addWidget(status);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    auto selected=[=]{return list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString();};
    auto selectionState=[=]{
        const auto item=entry(*library,selected());name->setText(item["name"].toString());collection->setText(item["category"].toString());insertButton->setEnabled(!item.isEmpty()&&!model->editMode);save->setEnabled(!item.isEmpty());
        const auto *object=model->active();const bool linked=object&&!object->style["component.id"].toString().isEmpty();saveLinks->setEnabled(linked&&!object->locked&&!model->editMode);
        publishButton->setEnabled(linked&&!object->locked&&!model->editMode&&object->style["component.role"].toString()=="Source");
        bool instances=false;for(const auto *candidate:model->selected())if(!candidate->locked&&candidate->style["component.role"].toString()=="Instance")instances=true;updateButton->setEnabled(instances&&!model->editMode);
        for(const auto &group:groups()){QSignalBlocker block(links[group]);links[group]->setEnabled(linked&&!object->locked);links[group]->setChecked(linked?object->style["component.links"].toObject()[group].toBool(true):true);}
        if(!linked){state->setText("Select a linked object in the scene to inspect its links.");return;}
        const auto source=entry(*library,object->style["component.id"].toString());const auto local=overrides(model->design,*object);
        state->setText(QString("%1 · %2 · linked revision %3 / source %4\nLocal overrides: %5").arg(object->name,object->style["component.role"].toString()).arg(object->style["component.revision"].toInt()).arg(source["revision"].toInt()).arg(local.isEmpty()?"none":local.join(", ")));
    };
    auto refresh=[=]{
        const auto id=selected(),previous=category->currentText();QStringList categories;auto items=entries(*library);for(auto value:items){const auto c=value.toObject()["category"].toString();if(!categories.contains(c))categories.append(c);}categories.sort(Qt::CaseInsensitive);
        {QSignalBlocker block(category);category->clear();category->addItem("All categories");category->addItems(categories);category->setCurrentIndex(qMax(0,category->findText(previous)));}
        QList<QJsonObject> sorted;for(auto value:items)sorted.append(value.toObject());std::stable_sort(sorted.begin(),sorted.end(),[=](const auto &a,const auto &b){if(recent->isChecked()&&a["lastUsed"]!=b["lastUsed"])return a["lastUsed"].toDouble()>b["lastUsed"].toDouble();return a["name"].toString().compare(b["name"].toString(),Qt::CaseInsensitive)<0;});
        QSignalBlocker block(list);list->clear();for(const auto &item:sorted){if(category->currentIndex()>0&&item["category"].toString()!=category->currentText())continue;if(!(item["name"].toString()+" "+item["category"].toString()).contains(search->text(),Qt::CaseInsensitive))continue;
            auto *row=new QListWidgetItem(QIcon(QPixmap::fromImage(thumbnail(item))),item["name"].toString()+"\n"+item["category"].toString()+QString(" · r%1").arg(item["revision"].toInt()),list);row->setData(Qt::UserRole,item["id"].toString());row->setToolTip("Stable component: "+item["id"].toString());if(item["id"].toString()==id)list->setCurrentItem(row);}
        selectionState();
    };
    QObject::connect(search,&QLineEdit::textChanged,dialog,[=]{refresh();});QObject::connect(category,&QComboBox::currentIndexChanged,dialog,[=]{refresh();});QObject::connect(recent,&QCheckBox::toggled,dialog,[=]{refresh();});QObject::connect(list,&QListWidget::currentItemChanged,dialog,[=]{selectionState();});
    auto insertSelected=[=]{QString error;const auto id=insert(*model,*library,selected(),&error);status->setText(id.isEmpty()?error:"Linked instance inserted. Its edits stay local until you explicitly update it from the source.");selectionState();};
    QObject::connect(insertButton,&QPushButton::clicked,dialog,insertSelected);QObject::connect(list,&QListWidget::itemDoubleClicked,dialog,[=]{insertSelected();});
    QObject::connect(publishButton,&QPushButton::clicked,dialog,[=]{QString error;if(!publish(*model,*library,&error)){status->setText(error);return;}status->setText("Source revision published. Select linked instances and review which changes to apply; existing copies stay unchanged until then.");selectionState();});
    QObject::connect(save,&QPushButton::clicked,dialog,[=]{QString error;status->setText(metadata(*library,selected(),name->text(),collection->text(),&error)?"Library details saved.":error);});
    QObject::connect(saveLinks,&QPushButton::clicked,dialog,[=]{QJsonObject values;for(const auto &group:groups())values[group]=links[group]->isChecked();QString error;status->setText(setLinks(*model,values,&error)?"Link settings saved. Unchecked groups will keep their local values.":error);selectionState();});
    QObject::connect(updateButton,&QPushButton::clicked,dialog,[=]{
        auto *review=roundedModal(dialog,"Apply source changes");review->setAttribute(Qt::WA_DeleteOnClose);review->setObjectName("componentUpdateReview");auto *body=new QVBoxLayout(review);body->setContentsMargins(16,16,16,16);
        auto *heading=new QLabel("Apply source changes");heading->setStyleSheet("font-size:16px;font-weight:600;background:transparent;");body->addWidget(heading);
        auto *explanation=new QLabel("Choose what to update on selected linked instances in this scene. Unlinked groups stay unchanged. Text, image content and placement stay local.");explanation->setWordWrap(true);body->addWidget(explanation);
        QStringList rows;for(const auto *object:model->selected()){if(object->style["component.role"].toString()!="Instance")continue;const auto record=entry(*library,object->style["component.id"].toString());rows.append(QString("%1: r%2 → r%3 · local: %4").arg(object->name).arg(object->style["component.revision"].toInt()).arg(record["revision"].toInt()).arg(overrides(model->design,*object).join(", ")));}auto *summary=new QLabel(rows.join('\n'));summary->setWordWrap(true);body->addWidget(summary);
        QHash<QString,QCheckBox*> chosen;for(const auto &group:groups()){auto *check=new QCheckBox(group=="Cues"?"Animation cues":group);check->setObjectName("componentUpdate"+group);check->setChecked(true);chosen[group]=check;body->addWidget(check);}
        auto *preserve=new QCheckBox("Preserve local overrides");preserve->setObjectName("componentPreserveOverrides");preserve->setChecked(true);body->addWidget(preserve);auto *errorLabel=new QLabel;errorLabel->setWordWrap(true);body->addWidget(errorLabel);
        auto *actions=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);body->addWidget(actions);const auto before=model->design.toJson();const auto selection=model->sel;const auto definitions=entries(*library);
        QObject::connect(actions->button(QDialogButtonBox::Apply),&QPushButton::clicked,review,[=]{if(before!=model->design.toJson()||selection!=model->sel||definitions!=entries(*library)){errorLabel->setText("The scene or source changed. Reopen this review before applying.");return;}QSet<QString> selectedGroups;for(const auto &group:groups())if(chosen[group]->isChecked())selectedGroups.insert(group);QString error;const auto count=updateSelected(*model,*library,selectedGroups,preserve->isChecked(),&error);if(count<=0){errorLabel->setText(error);return;}status->setText(QString("Updated %1 instance(s). Ctrl+Z restores the previous scene.").arg(count));review->accept();});
        QObject::connect(actions,&QDialogButtonBox::rejected,review,&QDialog::reject);review->resize(460,440);review->open();
    });
    QObject::connect(library,&Library::productionChanged,dialog,[=]{refresh();});QObject::connect(model,&BuildModel::selectionChanged,dialog,[=]{selectionState();});QObject::connect(model,&BuildModel::changed,dialog,[=]{selectionState();});
    refresh();search->setFocus();return dialog;
}
