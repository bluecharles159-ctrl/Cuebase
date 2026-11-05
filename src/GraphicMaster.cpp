#include "GraphicMaster.h"
#include "ElementLibrary.h"
#include "UiControls.h"
#include <QButtonGroup>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <array>
#include <algorithm>
#include <memory>

namespace {
const QStringList layouts{"Full screen","Lower third","Program ID"};
const QJsonObject defaults{{"title","The New Show"},{"schedule","Premieres tonight"},
    {"paper","#202d36"},{"accent","#e68656"},{"ink","#ffffff"},{"font","Segoe UI"},{"logo",""}};

ElementDesign makeLayout(int layout) {
    auto scene = ElementDesign::defaultScene();
    scene.camera()->cam.orthoScale = 16;
    scene.world.transparent = true;
    const bool lower = layout == 1;
    const bool ident = layout == 2;
    auto layer = [&](QString role, QString type, double w, double h, float x, float z) -> DObject& {
        DObject object;
        object.id = Library::newId();
        object.name = role;
        object.type = type;
        object.style["master.role"] = role;
        object.w = w;
        object.h = h;
        object.mat.shadeless = true;
        object.base.rot = {90,0,0};
        object.base.loc = {x,-0.015f * float(scene.objects.size()),z};
        scene.objects.append(object);
        return scene.objects.last();
    };
    layer("Panel","Shape",lower ? 13.2 : 16,lower ? 2.1 : 9,lower ? -1.f : 0.f,lower ? -2.8f : 0.f);
    layer("Accent","Shape",ident ? 1.1 : .09,ident ? .08 : (lower ? 1.45 : 3.15),
          ident ? 0.f : -7.05f,ident ? -1.55f : (lower ? -2.8f : -.4f));
    auto textLayer = [&](QString role, double h, float z, int size) {
        auto &o = layer(role,"Text",ident ? 14. : (lower ? 10. : 11.5),h,
                        ident ? 0.f : (lower ? -1.72f : -0.95f),z);
        o.fontPx = size;
        o.style["text.align"] = ident ? "Center" : "Left";
        o.style["text.weight"] = role == "Headline" ? 700 : 500;
        o.style["studio.editable"] = true;
        o.style["studio.label"] = role;
        o.style["studio.fit"] = true;
    };
    textLayer("Headline",lower ? .93 : 1.75,lower ? -2.5f : (ident ? .1f : -.1f),lower ? 76 : 125);
    textLayer("Schedule",.59,lower ? -3.29f : -1.08f,lower ? 38 : 45);
    layer("Logo","Image",lower ? 1.1 : 1.7,lower ? 1.1 : 1.7,
          ident ? 0.f : (lower ? 4.35f : -5.90f),lower ? -2.8f : 2.35f);
    return scene;
}

// Layout, keys and locally overridden token values belong to each variant.
void applyTokens(ElementDesign &scene,const QJsonObject &prior,const QJsonObject &next) {
    for (auto &o : scene.objects) {
        const auto role = o.style["master.role"].toString();
        QString color;
        if (role == "Panel") color = "paper";
        if (role == "Accent" || role == "Schedule") color = "accent";
        if (role == "Headline") color = "ink";
        if (!color.isEmpty() && (prior.isEmpty() || o.mat.color == QColor(prior[color].toString())))
            o.mat.color = QColor(next[color].toString());
        if (role == "Headline" || role == "Schedule") {
            const QString key = role == "Headline" ? "title" : "schedule";
            if (prior.isEmpty() || o.text == prior[key].toString()) o.text = next[key].toString();
            if (prior.isEmpty() || o.style["text.family"] == prior["font"]) o.style["text.family"] = next["font"];
        }
        if (role == "Logo" && (prior.isEmpty() || o.image == prior["logo"].toString())) {
            if (prior.isEmpty() || o.visible == !o.image.isEmpty()) o.visible = !next["logo"].toString().isEmpty();
            o.image = next["logo"].toString();
        }
        if (prior.isEmpty() && !role.isEmpty()) o.captureDefaults();
    }
}

Element variant(const Element *existing,int index,const QJsonObject &old,const QJsonObject &tokens) {
    Element e;
    if (existing) e = *existing;
    else {
        e.id = Library::newId();
        e.type = "Custom";
        e.playlist = "Master graphics";
        e.previewMs = 0;
    }
    auto scene = existing ? ElementDesign::fromJson(e.design) : makeLayout(index);
    applyTokens(scene,existing ? old : QJsonObject{},tokens);
    const auto suffix = " · " + layouts[index];
    if (!existing || e.name == old["title"].toString()+suffix) e.name = tokens["title"].toString()+suffix;
    e.design = scene.toJson();
    if (!existing) e.seconds = qMax(1,(scene.durationMs+999)/1000);
    return e;
}

void editMaster(Library *library,QWidget *parent,QString id,const std::function<bool()> &saveDraft,
                const std::function<void(QString)> &openVariant) {
    const auto original = library->production["graphicMasters"].toObject().value(id).toObject();
    if (!id.isEmpty() && original.isEmpty()) return;
    const auto old = original["tokens"].toObject();
    auto tokens = old.isEmpty() ? defaults : old;
    const auto ids = original["variants"].toArray();
    std::unique_ptr<QDialog> dialog(roundedModal(parent,original.isEmpty() ? "New master graphic" : "Edit master graphic"));
    dialog->setObjectName("graphicMasterDialog");
    dialog->setMinimumWidth(650);
    auto *body = new QVBoxLayout(dialog.get());
    body->setContentsMargins(20,18,20,18);
    body->setSpacing(12);
    auto *heading = new QLabel(dialog->windowTitle());
    QFont headingFont = heading->font(); headingFont.setPointSize(headingFont.pointSize()+3); headingFont.setBold(true);
    heading->setFont(headingFont); body->addWidget(heading);
    auto *previews = new QHBoxLayout;
    auto *selection = new QButtonGroup(dialog.get());
    std::array<ElementPreview*,3> pictures;
    for (int i=0;i<layouts.size();++i) {
        auto *col = new QVBoxLayout;
        pictures[i] = new ElementPreview;
        pictures[i]->setMinimumSize(150,88);
        auto *button = new QPushButton(layouts[i]);
        button->setCheckable(true); button->setObjectName("masterVariant"+QString::number(i));
        selection->addButton(button,i); button->setChecked(i==0);
        col->addWidget(pictures[i]); col->addWidget(button); previews->addLayout(col,1);
    }
    body->addLayout(previews);
    auto *fields = new QFormLayout;
    auto *title = new QLineEdit(tokens["title"].toString());
    title->setObjectName("masterHeadline"); title->setMaxLength(240);
    auto *schedule = new QLineEdit(tokens["schedule"].toString());
    schedule->setObjectName("masterSchedule"); schedule->setMaxLength(240);
    fields->addRow("Headline",title); fields->addRow("Schedule line",schedule);
    body->addLayout(fields);
    auto *brand = new QGroupBox("Shared branding");
    auto *brandFields = new QFormLayout(brand);
    auto *font = new QFontComboBox;
    font->setCurrentFont(QFont(tokens["font"].toString())); brandFields->addRow("Typeface",font);
    auto *colors = new QHBoxLayout;
    QTimer refresh;
    refresh.setSingleShot(true); refresh.setInterval(65);
    auto update = [&] {
        for (int i=0;i<layouts.size();++i) {
            const auto *e = i<ids.size() ? library->element(ids[i].toString()) : nullptr;
            pictures[i]->setElement(variant(e,i,old,tokens));
        }
    };
    QObject::connect(&refresh,&QTimer::timeout,dialog.get(),update);
    const QMap<QString,QString> swatches{{"accent","Accent"},{"ink","Text"},{"paper","Panel"}};
    for (auto it=swatches.cbegin();it!=swatches.cend();++it) {
        const auto key = it.key();
        auto *button = new QPushButton(it.value());
        auto setChip = [&,key,button] {
            QPixmap sample(16,16); sample.fill(QColor(tokens[key].toString()));
            button->setIcon(QIcon(sample));
        };
        setChip(); colors->addWidget(button);
        QObject::connect(button,&QPushButton::clicked,dialog.get(),[&,key,button,setChip] {
            const auto chosen = QColorDialog::getColor(QColor(tokens[key].toString()),dialog.get(),button->text());
            if (chosen.isValid()) { tokens[key]=chosen.name(); setChip(); refresh.start(); }
        });
    }
    brandFields->addRow("Colors",colors);
    auto *logoRow = new QHBoxLayout;
    auto *logo = new QLineEdit(tokens["logo"].toString());
    logo->setClearButtonEnabled(true); logo->setPlaceholderText("Optional logo image");
    auto *browse = new QPushButton("Browse…");
    logoRow->addWidget(logo); logoRow->addWidget(browse); brandFields->addRow("Logo",logoRow);
    body->addWidget(brand);
    QObject::connect(browse,&QPushButton::clicked,dialog.get(),[&] {
        const auto path = QFileDialog::getOpenFileName(dialog.get(),"Choose logo",logo->text(),"Images (*.png *.jpg *.jpeg *.webp *.svg)");
        if (!path.isEmpty()) logo->setText(path);
    });
    auto bind = [&](QLineEdit *edit,QString key) {
        QObject::connect(edit,&QLineEdit::textChanged,dialog.get(),[&,key](const QString &value) {
            tokens[key]=value.trimmed(); refresh.start();
        });
    };
    bind(title,"title"); bind(schedule,"schedule"); bind(logo,"logo");
    QObject::connect(font,&QFontComboBox::currentFontChanged,dialog.get(),[&](const QFont &f){tokens["font"]=f.family();refresh.start();});
    auto *error = new QLabel; error->setWordWrap(true); error->hide(); body->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(original.isEmpty() ? "Create three variants" : "Update variants");
    body->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog.get(),&QDialog::reject);
    QString openId;
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog.get(),[&] {
        auto fail = [&](const QString &message){error->setText(message);error->show();};
        if (tokens["title"].toString().isEmpty()) {fail("Enter a headline."); title->setFocus(); return;}
        if (tokens["logo"]!=old["logo"] && !tokens["logo"].toString().isEmpty() && !QImageReader(tokens["logo"].toString()).canRead()) {
            fail("Choose a readable logo image."); return;
        }
        if (!saveDraft()) {fail("Save your current Build scene to open a variant.");return;}
        auto masters = library->production["graphicMasters"].toObject();
        if (!id.isEmpty() && masters.value(id).toObject()!=original) {fail("This master changed. Reopen it to use its latest settings.");return;}
        const auto before = library->elements;
        const auto playlists = library->elementPlaylists;
        const auto production = library->production;
        QJsonArray linked;
        for (int i=0;i<layouts.size();++i) {
            const auto elementId = i<ids.size() ? ids[i].toString() : QString();
            auto it = std::find_if(library->elements.begin(),library->elements.end(),[&](const Element &e){return e.id==elementId;});
            const bool found = it!=library->elements.end();
            auto result = variant(found ? &*it : nullptr,i,old,tokens);
            refreshElementThumbnail(result); linked.append(result.id);
            if (found) *it=result; else library->elements.append(result);
        }
        if (!library->elementPlaylists.contains("Master graphics")) library->elementPlaylists.append("Master graphics");
        masters.insert(id.isEmpty() ? Library::newId() : id,QJsonObject{{"tokens",tokens},{"variants",linked}});
        library->production["graphicMasters"]=masters;
        QString message;
        if (!library->saveChecked(&message)) {
            library->elements=before; library->elementPlaylists=playlists; library->production=production;
            fail(message); return;
        }
        openId=linked[selection->checkedId()].toString();
        emit library->elementsChanged(); emit library->productionChanged(); dialog->accept();
    });
    update();
    if (dialog->exec()==QDialog::Accepted && !openId.isEmpty()) openVariant(openId);
}
}

void populateGraphicMasters(QMenu *menu,Library *library,QWidget *parent,
                            std::function<bool()> saveDraft,std::function<void(QString)> openVariant) {
    QObject::connect(menu,&QMenu::aboutToShow,menu,[=] {
        menu->clear();
        auto add = [&](QString label,QString id) {
            auto *action = menu->addAction(label);
            QObject::connect(action,&QAction::triggered,menu,[=] {
                editMaster(library,parent,id,saveDraft,openVariant);
            });
        };
        add("New master graphic…",{});
        const auto masters = library->production["graphicMasters"].toObject();
        if (!masters.isEmpty()) menu->addSeparator();
        for (auto it=masters.begin();it!=masters.end();++it)
            add(it.value().toObject()["tokens"].toObject()["title"].toString(),it.key());
    });
}
