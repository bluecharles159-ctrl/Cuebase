#include <QSignalBlocker>
#include "UiControls.h"
#include "BuildWorkspace.h"
#include "TextureLibrary.h"
#include "ComponentLibrary.h"
#include "FrameExport.h"
#include "ColorManagement.h"
#include <QProgressDialog>
#include <QApplication>
#include "CueSequence.h"
#include <QRegularExpression>
#include <QJsonDocument>
#include "ElementLibrary.h"

#include <QCheckBox>
#include "ColorPicker.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QPointer>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <functional>
#include <memory>

#include "BuildCanvas.h"
#include "BuildModel.h"
#include "Data.h"
#include "AnimationEditor.h"
#include "AnimationCuePanel.h"
#include "PropertyInspector.h"
#include "LayerPanel.h"
#include <QTreeWidgetItemIterator>
#include "AssetImport.h"
#include "DesignAssets.h"
#include "SceneEffects.h"
#include "DesignAudio.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSettings>
#include <QMessageBox>

namespace {

class CompositionPreview final : public QWidget {
public:
    CompositionPreview(BuildModel *model,Library *library):m(model),lib(library){
        setObjectName("compositionCleanPreview");
        setMinimumSize(120,100);setToolTip("Clean broadcast composition preview");
        connect(m,&BuildModel::changed,this,qOverload<>(&QWidget::update));
        connect(m,&BuildModel::timeChanged,this,[this]{update();});
        connect(lib,&Library::displayChanged,this,qOverload<>(&QWidget::update));
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#262626"));
        QSizeF size=QSizeF(width()-16,height()-16);const double aspect=lib->display.aspect();
        if(size.width()/size.height()>aspect)size.setWidth(size.height()*aspect);else size.setHeight(size.width()/aspect);
        QRectF frame(QPointF((width()-size.width())/2,(height()-size.height())/2),size);
        p.fillRect(frame,QColor("#393939"));
        QSizeF pixels=frame.size()*devicePixelRatioF();if(pixels.width()*pixels.height()>2073600)pixels*=qSqrt(2073600/(pixels.width()*pixels.height()));
        const QSize output(qMax(2,int(pixels.width())),qMax(2,int(pixels.height())));
        QImage image(output,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
        const auto &display=lib->display;
        if(sceneEffect(m->design))image=sceneEffectPreview(m->design,output,m->t);
        else{QPainter render(&image);render.setRenderHint(QPainter::Antialiasing);render.setRenderHint(QPainter::SmoothPixmapTransform);RenderSettings settings;settings.motionBlur=display.motionBlur;settings.shutter=display.shutter;settings.samples=display.mbSamples;settings.frameMs=1000.*display.fpsBase/qMax(1,display.fps);m->design.render(render,image.rect(),m->t,true,&settings);}
        if(colorManagementActive(display.colorView,display.colorLook,display.exposure,display.gamma))applyColorManagement(image,display.colorView,display.colorLook,display.exposure,display.gamma);
        p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(frame,image);
    }
private:BuildModel *m;Library *lib;
};

constexpr int kGutter = 150, kRuler = 26, kRow = 24;

QString clock(qint64 ms) {
    return QString("%1:%2.%3").arg(ms / 60000).arg(ms / 1000 % 60, 2, 10, QChar('0')).arg(ms % 1000 / 10, 2, 10, QChar('0'));
}

QLabel *caption(const QString &t) {
    auto *l = new QLabel(t);
    l->setStyleSheet("color:palette(placeholder-text); font-weight:700; font-size:10px; padding-top:4px;");
    return l;
}

QDoubleSpinBox *dspin(double lo, double hi, double step, int dec, const QString &suffix = QString()) {
    auto *s = new ValueInput;
    s->setRange(lo, hi);
    s->setSingleStep(step);
    s->setDecimals(dec);
    s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    return s;
}

QWidget *rowOf(const QList<QWidget *> &ws) {
    auto *w = new QWidget;
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(4);
    for (QWidget *x : ws) {
        x->setMinimumWidth(46);  // three of them have to fit side by side in the side panel
        l->addWidget(x, 1);
    }
    return w;
}

void paintColorButton(QPushButton *b, const QColor &c) {
    b->setStyleSheet(QString("background:%1; color:%2;").arg(c.name(), c.lightness() > 140 ? "#000" : "#fff"));
    b->setText(c.name());
}

QFrame *box(const QString &title, QVBoxLayout **inner) {
    auto *f = new QFrame;
    f->setObjectName("bBox");
    f->setStyleSheet("QFrame#bBox { background:transparent; border:0; }");
    auto *l = new QVBoxLayout(f);
    l->setContentsMargins(4, 4, 4, 4);
    l->setSpacing(3);
    auto *c = new QToolButton;
    c->setProperty("sectionHeader",true);c->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    const QString expansionKey="build/sections/"+title;const bool expanded=QSettings().value(expansionKey,title=="TRANSFORM").toBool();
    c->setText(title); c->setCheckable(true); c->setChecked(expanded);
    c->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); c->setIcon(uiIcon(expanded?"down":"right"));c->setIconSize(QSize(12,12));
    l->addWidget(c);
    auto *content = new QWidget;
    auto *contents = new QVBoxLayout(content); contents->setContentsMargins(18,2,5,8); contents->setSpacing(4);
    l->addWidget(content);
    content->setVisible(expanded);
    QObject::connect(c, &QToolButton::toggled, content, [c, content,expansionKey](bool on) {content->setVisible(on);c->setIcon(uiIcon(on ? "down" : "right"));QSettings().setValue(expansionKey,on);});
    *inner = contents;
    return f;
}

QWidget *scrollPage(QWidget *content) {
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setWidget(content);
    return sa;
}

} // namespace

// ============================================================ the workspace

BuildWorkspace::BuildWorkspace(Library *lib, QWidget *parent) : QWidget(parent), m_lib(lib) {
    setStyleSheet("BuildWorkspace{background:palette(window);}");
    m_model = new BuildModel;
    m_model->setParent(this);
    BuildModel *m = m_model;
    m->fps = lib->display.fps;
    m->fpsBase = lib->display.fpsBase;
    connect(lib, &Library::displayChanged, this, [this, lib] {
        m_model->fps = lib->display.fps;
        m_model->fpsBase = lib->display.fpsBase;
    });
    m_canvas = new BuildCanvas(m, lib);
    BuildCanvas *canvas = m_canvas;

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // ---------------------------------------------------------------- top bar
    auto *bar = new QHBoxLayout;
    auto *back = new QPushButton("←  Studio");
    auto *title = new QLabel("BUILD");
    title->setStyleSheet("font-weight:800; font-size:14px; color:#60a5fa; letter-spacing:2px;");
    m_elementPicker=new QComboBox;m_elementPicker->setObjectName("buildElementPicker");m_elementPicker->setEditable(true);m_name=m_elementPicker->lineEdit();connect(m_name,&QLineEdit::textEdited,this,[this](const QString &value){m_draftName=value;});
    m_name->setPlaceholderText("Element name");
    m_name->setMinimumWidth(220);
    m_msg = new QLabel;
    m_msg->setStyleSheet("color:palette(placeholder-text);");
    auto *undoBtn = new QPushButton("↶");
    auto *redoBtn = new QPushButton("↷");
    undoBtn->setToolTip("Undo (Ctrl+Z)");
    redoBtn->setToolTip("Redo (Ctrl+Shift+Z)");
    undoBtn->setFixedWidth(36);
    redoBtn->setFixedWidth(36);
    auto *saveBtn = new QPushButton("Save to Screen Elements");
    saveBtn->setObjectName("buildSave");
    bar->addWidget(back);
    bar->addSpacing(6);
    bar->addWidget(title);
    bar->addSpacing(10);
    bar->addWidget(m_elementPicker);
    auto *previewButton=new QPushButton(uiIcon("image"),"Preview");previewButton->setToolTip("Capture or edit the library thumbnail");bar->addWidget(previewButton);connect(previewButton,&QPushButton::clicked,this,&BuildWorkspace::capturePreview);
    connect(m_elementPicker,qOverload<int>(&QComboBox::activated),this,[this](int index){const QString id=m_elementPicker->itemData(index).toString();if(id.isEmpty()||(id==m_editingId&&m_cueIteration<0))return;m_name->setText(m_draftName);if(hasUnsavedChanges()){save();if(hasUnsavedChanges())return;}editElement(id);});
    connect(m_lib,&Library::elementsChanged,this,&BuildWorkspace::refreshElements);
    bar->addWidget(m_msg, 1);
    bar->addWidget(undoBtn);
    bar->addWidget(redoBtn);
    bar->addWidget(saveBtn);
    root->addLayout(bar);
    connect(m,&BuildModel::componentRegistrationRequested,this,[this,m,lib](const QString &id,const QString &name){QString error;const auto component=Components::registerSource(*m,*lib,id,name,&error);say(component.isEmpty()?error:"Reusable source registered · press / to insert a linked instance",component.isEmpty());});
    auto openComponents=[this,m,lib]{if(auto *existing=findChild<QDialog*>("componentBrowser")){existing->show();existing->raise();existing->activateWindow();return;}Components::createBrowser(m,lib,this)->show();};
    connect(canvas,&BuildCanvas::componentBrowserRequested,this,openComponents);
    auto *componentsButton=new QPushButton(uiIcon("component"),"Components");componentsButton->setObjectName("buildComponents");componentsButton->setToolTip("Reusable objects · / in the viewport · rename ::name to register");bar->insertWidget(5,componentsButton);connect(componentsButton,&QPushButton::clicked,this,openComponents);

    // ---------------------------------------------------------------- viewport toolbar
    auto *tb = new QVBoxLayout;
    tb->setSpacing(6);
    auto *addBtn = new QPushButton("＋  Add");
    addBtn->setObjectName("save");
    addBtn->setToolTip("Add a mesh, text, image or light (Shift+A in the viewport)");
    auto *editBtn = new QPushButton("Edit Mode");
    editBtn->setCheckable(true);
    editBtn->setToolTip("Edit the selected mesh: vertices, edges and faces (Tab)");
    auto *objBtn = new QToolButton;
    objBtn->setText("Object ▾");
    objBtn->setPopupMode(QToolButton::InstantPopup);
    auto *meshBtn = new QToolButton;
    meshBtn->setText("Mesh ▾");
    meshBtn->setPopupMode(QToolButton::InstantPopup);
    auto *vBtn = new QPushButton("Vertex"), *eBtn = new QPushButton("Edge"), *fBtn = new QPushButton("Face");
    for (QPushButton *b : {vBtn, eBtn, fBtn}) {
        b->setCheckable(true);
        b->setAutoExclusive(true);
    }
    vBtn->setToolTip("Vertex select (1)");
    eBtn->setToolTip("Edge select (2)");
    fBtn->setToolTip("Face select (3)");
    auto *wireBtn = new QPushButton("Wireframe"), *solidBtn = new QPushButton("Work"), *rendBtn = new QPushButton("Final"), *materialBtn=new QPushButton("Material");
    for (QPushButton *b : {wireBtn, solidBtn, rendBtn, materialBtn}) {
        b->setCheckable(true);
        b->setAutoExclusive(true);
    }
    rendBtn->setChecked(true);
    auto *fixedBtn = new QPushButton("2D Composition");
    fixedBtn->setCheckable(false);
    fixedBtn->setChecked(true);
    fixedBtn->setToolTip("Fixed broadcast camera composition");
    auto *fitBtn = new QPushButton("Fit");
    tb->addWidget(addBtn);
    tb->addWidget(editBtn);
    tb->addWidget(objBtn);
    tb->addWidget(meshBtn);
    tb->addWidget(vBtn);
    tb->addWidget(eBtn);
    tb->addWidget(fBtn);
    tb->addStretch();
    tb->addWidget(wireBtn);
    tb->addWidget(solidBtn);
    tb->addWidget(materialBtn);
    tb->addWidget(rendBtn);
    tb->addSpacing(8);
    tb->addWidget(fixedBtn);
    tb->addWidget(fitBtn);
    auto *actual = new QPushButton("100%");
    tb->addWidget(actual);
    connect(actual, &QPushButton::clicked, canvas, &BuildCanvas::actualSize);
    auto *helpers = new QToolButton;
    helpers->setText("View / Align");helpers->setPopupMode(QToolButton::InstantPopup);
    auto *helperMenu = new QMenu(helpers);
    auto overlay = [&](const QString &label, bool *value) {
        auto *action=helperMenu->addAction(label);action->setCheckable(true);action->setChecked(*value);
        connect(action,&QAction::toggled,canvas,[canvas,value](bool on){*value=on;canvas->update();});
    };
    overlay("Title safe",&canvas->showTitleSafe);overlay("Action safe",&canvas->showActionSafe);
    overlay("Object origins",&canvas->showOrigins);
    overlay("All overlays",&canvas->showOverlays);overlay("Placement cursor",&canvas->showCursor);overlay("Lights",&canvas->showLights);overlay("Camera",&canvas->showCamera);
    overlay("Center guides",&canvas->showAxes);overlay("Grid",&canvas->showGrid);overlay("Rulers · drag to create guides",&canvas->showRulers);
    overlay("Snap · Alt temporarily bypasses",&canvas->snapping);
    auto *guideSettings=helperMenu->addAction("Composition guides…");guideSettings->setObjectName("compositionGuideSettings");
    connect(guideSettings,&QAction::triggered,this,[=,this]{
        const auto *camera=m->design.camera();if(!camera)return;const auto cameraId=camera->id;const auto original=camera->style;
        std::unique_ptr<QDialog> dialog(roundedModal(this,"Composition guides"));dialog->setObjectName("compositionGuideDialog");auto *form=new QFormLayout(dialog.get());form->setContentsMargins(16,16,16,16);
        auto *heading=new QLabel("Composition guides");heading->setStyleSheet("font-size:14px;font-weight:600;");form->addRow(heading);
        auto *title=dspin(0,45,.5,1," %"),*action=dspin(0,45,.5,1," %"),*grid=dspin(1,4096,1,0," px");
        title->setObjectName("guideTitleMargin");action->setObjectName("guideActionMargin");grid->setObjectName("guideGridSpacing");title->setValue(original["editor.titleSafe"].toDouble(.05)*100);action->setValue(original["editor.actionSafe"].toDouble(.035)*100);grid->setValue(original["editor.gridSpacing"].toDouble(120));
        form->addRow("Title-safe inset",title);form->addRow("Action-safe inset",action);form->addRow("Grid spacing",grid);
        auto *color=new QLineEdit(original["editor.guideColor"].toString("#22d3ee"));color->setObjectName("compositionGuideColor");color->setToolTip("Guide color as #RRGGBB or #AARRGGBB");form->addRow("Guide color",color);
        auto *hint=new QLabel("Drag from a ruler to add a guide. Drag outside the composition to remove it. Guides save with this scene and never appear in output.");hint->setWordWrap(true);form->addRow(hint);
        auto *error=new QLabel;error->setWordWrap(true);form->addRow(error);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::rejected,dialog.get(),&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,dialog.get(),[&]{if(!QColor(color->text()).isValid()){error->setText("Enter a valid guide color.");return;}dialog->accept();});
        if(dialog->exec()!=QDialog::Accepted)return;auto *current=m->design.find(cameraId);if(!current||current->style!=original){say("Composition settings changed. Reopen guide settings.",true);return;}
        auto updated=original;updated["editor.titleSafe"]=title->value()/100;updated["editor.actionSafe"]=action->value()/100;updated["editor.gridSpacing"]=grid->value();updated["editor.guideColor"]=QColor(color->text()).name(QColor::HexArgb);
        if(updated!=original){m->pushUndo();current->style=updated;m->notify();}
    });
    auto *clearGuides=helperMenu->addAction("Clear guides");clearGuides->setObjectName("clearCompositionGuides");connect(clearGuides,&QAction::triggered,canvas,[=]{if(auto *camera=m->design.camera()){if(camera->style["editor.guides.vertical"].toArray().isEmpty()&&camera->style["editor.guides.horizontal"].toArray().isEmpty())return;m->pushUndo();camera->style.remove("editor.guides.vertical");camera->style.remove("editor.guides.horizontal");m->notify();}canvas->clearGuides();});
    helperMenu->addSeparator();
    for(const QString &op:QStringList{"Left","Horizontal Center","Right","Top","Vertical Center","Bottom","Distribute Horizontal","Distribute Vertical"})
        helperMenu->addAction(op,canvas,[canvas,op]{canvas->alignSelection(op);});
    helpers->setMenu(helperMenu);tb->addWidget(helpers);
    auto *cursorTool=new QToolButton;cursorTool->setIcon(uiIcon("cursor"));cursorTool->setToolTip("Set placement cursor · Shift+right-click");cursorTool->setCheckable(true);tb->insertWidget(1,cursorTool);
    connect(cursorTool,&QToolButton::toggled,canvas,[canvas](bool enabled){canvas->placeCursor=enabled;canvas->setCursor(enabled?Qt::CrossCursor:Qt::ArrowCursor);});
    helperMenu->addSeparator();helperMenu->addAction("Cursor to composition center",this,[m]{m->pushUndo();m->design.cursor={};m->notify();});
    helperMenu->addAction("Cursor to active object",this,[m]{if(auto *object=m->active()){m->pushUndo();m->design.cursor=m->design.worldMatrix(*object,m->t).map(QVector3D());m->notify();}});

    // ---------------------------------------------------------------- the animation timeline
    auto *tlFrame = new QFrame;
    tlFrame->setObjectName("panel");
    auto *tl = new QVBoxLayout(tlFrame);
    tl->setContentsMargins(4, 4, 4, 4);
    tl->setSpacing(3);
    auto *tr = new QHBoxLayout;
    auto *toStart = new QPushButton("⏮");
    toStart->setToolTip("Back to the start (Shift+←)");
    auto *play = new QPushButton("▶  Play");
    play->setFixedWidth(96);
    play->setToolTip("Play / pause (Space)");
    auto *loop = new QCheckBox("Loop");
    loop->setChecked(true);
    auto *timeLabel = new QLabel;timeLabel->setMinimumWidth(40);timeLabel->setAlignment(Qt::AlignCenter);
    timeLabel->setStyleSheet("font-family:Consolas; font-weight:700;");
    auto *interp = new QComboBox;
    interp->addItems(interpolationKinds());
    interp->setToolTip("How the move from the selected keyframe to the next one eases");
    auto *keyBtn = new QPushButton("◆  Keyframe");
    keyBtn->setToolTip("Keep the selected objects' current look as a keyframe (K)");
    auto *delKey = new QPushButton("✕");
    delKey->setToolTip("Delete the selected keyframe (Alt+K)");
    delKey->setFixedWidth(34);
    auto *autoKey = new QCheckBox("Auto Key");
    autoKey->setChecked(false);
    autoKey->setToolTip("Automatically key changed animatable properties. Off by default.");
    auto *graphBtn = new QPushButton("Graph Editor");
    auto *dur = dspin(0.5, 120, 0.5, 1, " s");
    dur->setValue(5.0);
    tr->addWidget(caption("ANIMATION"));
    tr->addSpacing(6);
    tr->addWidget(toStart);
    tr->addWidget(play);
    tr->addWidget(loop);
    tr->addSpacing(6);
    tr->addWidget(timeLabel);
    tr->addStretch();
    tr->addWidget(autoKey);
    dur->hide();
    auto *startFrame=new FrameInput,*endFrame=new FrameInput;startFrame->setObjectName("rangeStart");endFrame->setObjectName("rangeEnd");
    startFrame->setRange(0,100000);endFrame->setRange(1,100001);endFrame->setValue(m->design.durationMs/m->frameMs());
    startFrame->setToolTip("Playback start frame");endFrame->setToolTip("Playback end frame");tr->addWidget(new QLabel("Start"));tr->addWidget(startFrame);tr->addWidget(new QLabel("End"));tr->addWidget(endFrame);
    connect(startFrame,&QSpinBox::valueChanged,this,[m,endFrame](int value){if(value>=endFrame->value())return;m->pushUndo();m->design.startMs=value*m->frameMs();m->notify();});
    connect(endFrame,&QSpinBox::valueChanged,this,[m,startFrame](int value){if(value<=startFrame->value())return;m->pushUndo();m->design.durationMs=value*m->frameMs();m->setTime(qMin(m->t,qint64(m->design.durationMs)));m->notify();});
    connect(m,&BuildModel::changed,this,[m,startFrame,endFrame]{QSignalBlocker a(startFrame),b(endFrame);startFrame->setMaximum(qMax(0,m->design.durationMs/m->frameMs()-1));endFrame->setMinimum(m->design.startMs/m->frameMs()+1);startFrame->setValue(m->design.startMs/m->frameMs());endFrame->setValue(m->design.durationMs/m->frameMs());});
    tl->addLayout(tr);
    auto *keyRow=new QHBoxLayout;
    keyRow->addWidget(keyBtn);keyRow->addWidget(delKey);keyRow->addWidget(interp);
    keyRow->addStretch();keyRow->addWidget(graphBtn);tl->addLayout(keyRow);
    keyBtn->setText("Key Transform");
    auto *tracks = new AnimationEditor(m, false);
    auto *graph = new AnimationEditor(m, true);
    auto *editorStack = new QStackedWidget;
    auto *tsa = new QScrollArea;
    tsa->setWidgetResizable(true);
    tsa->setFrameShape(QFrame::NoFrame);
    tsa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tsa->setWidget(tracks);
    editorStack->addWidget(tsa);
    editorStack->addWidget(graph);
    tl->addWidget(editorStack, 1);
    connect(graphBtn, &QPushButton::clicked, this, [editorStack, graphBtn] {
        const bool showGraph = editorStack->currentIndex() == 0;
        editorStack->setCurrentIndex(showGraph ? 1 : 0);
        graphBtn->setToolTip(showGraph ? "Timeline" : "Graph Editor");
    });

    auto *leftCol = new QWidget;
    auto *lc = new QVBoxLayout(leftCol);
    lc->setContentsMargins(0, 0, 0, 0);
    lc->setSpacing(6);
    auto *canvasRow=new QHBoxLayout;canvasRow->setSpacing(4);canvasRow->addLayout(tb);
    auto *left = new QSplitter(Qt::Vertical);
    auto *views=new QSplitter(Qt::Horizontal);
    views->addWidget(new CompositionPreview(m,lib));views->addWidget(canvas);views->setSizes({420,640});
    auto *lower=new QSplitter(Qt::Horizontal);
    auto *layers=new LayerPanel(m);layers->setMaximumWidth(QWIDGETSIZE_MAX);
    auto *layerPanel=new QWidget;auto *layerLayout=new QVBoxLayout(layerPanel);layerLayout->setContentsMargins(0,0,0,0);layerLayout->setSpacing(2);
    auto *layerBar=new QHBoxLayout;auto *searchButton=new QToolButton;searchButton->setIcon(uiIcon("100%"));searchButton->setToolTip("Search layers");auto *reveal=new QToolButton;reveal->setIcon(uiIcon("composition"));reveal->setToolTip("Reveal active layer");auto *search=new QLineEdit;search->setPlaceholderText("Search layers");search->hide();layerBar->addWidget(searchButton);layerBar->addWidget(reveal);layerBar->addWidget(search,1);layerLayout->addLayout(layerBar);layerLayout->addWidget(layers,1);
    connect(searchButton,&QToolButton::clicked,search,[search]{search->setVisible(!search->isVisible());if(search->isVisible())search->setFocus();else search->clear();});
    connect(reveal,&QToolButton::clicked,layers,[layers,m,search]{search->clear();for(QTreeWidgetItemIterator it(layers);*it;++it)if((*it)->data(0,Qt::UserRole).toString()==m->activeId){for(auto *parent=(*it)->parent();parent;parent=parent->parent())parent->setExpanded(true);layers->scrollToItem(*it,QAbstractItemView::PositionAtCenter);break;}});
    auto filterLayers=[layers,search]{const auto text=search->text();for(QTreeWidgetItemIterator it(layers);*it;++it)(*it)->setHidden(!text.isEmpty()&&!(*it)->text(0).contains(text,Qt::CaseInsensitive));if(!text.isEmpty())for(QTreeWidgetItemIterator it(layers);*it;++it)if(!(*it)->isHidden())for(auto *parent=(*it)->parent();parent;parent=parent->parent()){parent->setHidden(false);parent->setExpanded(true);}};
    connect(search,&QLineEdit::textChanged,layers,filterLayers);connect(m,&BuildModel::changed,layers,filterLayers);
    lower->addWidget(layerPanel);lower->addWidget(tlFrame);lower->setSizes({240,820});
    for(auto *split:{views,lower}){split->setHandleWidth(4);split->setChildrenCollapsible(false);}
    left->addWidget(views);
    left->addWidget(lower);
    left->setChildrenCollapsible(false);
    left->setHandleWidth(8);
    left->setSizes({520, 220});
    canvasRow->addWidget(left,1);lc->addLayout(canvasRow,1);

    // ---------------------------------------------------------------- the side panel
    auto *side = new QFrame;
    side->setObjectName("panel");
    side->setMinimumWidth(280);
    side->setMaximumWidth(QWIDGETSIZE_MAX);
    auto *sideL = new QHBoxLayout(side);
    sideL->setContentsMargins(8, 8, 4, 8);
    sideL->setSpacing(6);
    auto *tabs = new QVBoxLayout;
    tabs->setSpacing(4);
    const QStringList tabNames{"Inspector", "Modifiers", "Material", "World", "Animation Cues", "Effects"};
    QVector<QPushButton *> tabBtns;
    for (const QString &n : tabNames) {
        auto *b = new QPushButton(n);
        b->setObjectName("buildTab_"+n);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setStyleSheet("padding:6px 4px;");
        tabs->addWidget(b);
        tabBtns << b;
    }
    tabBtns[0]->setChecked(true);
    tabBtns[2]->setToolTip("Materials · textures and reusable surface presets");
    tabs->addStretch();sideL->addLayout(tabs);
    auto *stack = new QStackedWidget;
    stack->setObjectName("buildPropertyPages");
    sideL->addWidget(stack, 1);
    for (int i = 0; i < tabBtns.size(); ++i) connect(tabBtns[i], &QPushButton::clicked, stack, [stack, i] { stack->setCurrentIndex(i); });
    connect(m,&BuildModel::cueEditorRequested,this,[=]{tabBtns[4]->click();});

    auto *body = new QHBoxLayout;
    auto *panels=new QSplitter(Qt::Horizontal);panels->setHandleWidth(4);panels->setChildrenCollapsible(false);
    panels->addWidget(leftCol);panels->addWidget(side);panels->setSizes({1060,300});
    body->addWidget(panels,1);
    root->addLayout(body, 1);
    auto *sceneBar=new QHBoxLayout;auto *newScene=new QPushButton(uiIcon("add"),"New scene");newScene->setObjectName("newBuildScene");newScene->setToolTip("Save this scene and start a new one");sceneBar->addWidget(newScene);
    connect(newScene,&QPushButton::clicked,this,[this]{if(hasUnsavedChanges()||m_editingId.isEmpty()){save();if(hasUnsavedChanges()||m_editingId.isEmpty())return;}newElement();});
    auto *presets=new QToolButton;presets->setText("Presets");presets->setIcon(uiIcon("composition"));presets->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);presets->setPopupMode(QToolButton::InstantPopup);auto *presetMenu=new QMenu(presets);
    for(const auto &name:elementPresetNames()){auto *action=presetMenu->addAction(name);connect(action,&QAction::triggered,this,[this,name]{if(hasUnsavedChanges()){save();if(hasUnsavedChanges())return;}newElement();m_name->setText(name);m_draftName=name;m_model->reset(elementPreset(name));m_canvas->frameAll();});}presets->setMenu(presetMenu);sceneBar->addWidget(presets);sceneBar->addStretch();root->addLayout(sceneBar);
    auto *hints=new QLabel;hints->setStyleSheet("color:palette(placeholder-text);font-size:10px;padding:2px;");root->addWidget(hints);
    connect(canvas,&BuildCanvas::shortcutHint,hints,&QLabel::setText);

    auto syncing = std::make_shared<bool>(false);
    auto renaming = std::make_shared<bool>(false);
    auto lastTag = std::make_shared<QString>();
    auto clk = std::make_shared<QElapsedTimer>();
    clk->start();
    // one undo step per burst of changes to the same field
    auto touch = [=, this](const QString &tag) {
        if (*lastTag != tag || clk->elapsed() > 1000) {
            m->pushUndo();
            *lastTag = tag;
        }
        clk->restart();
    };

    // ================================================================ OBJECT tab
    auto *objPage = new QWidget;
    auto *ol = new QVBoxLayout(objPage);
    ol->setContentsMargins(4, 4, 8, 8);
    ol->setSpacing(8);
    ol->addWidget(caption("OBJECTS   (double-click a name to rename)"));
    auto *list = new QListWidget;
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    list->setMinimumHeight(120);
    list->setMaximumHeight(170);
    ol->addWidget(list);
    auto *actRow = new QHBoxLayout;
    auto *dupBtn = new QPushButton("Duplicate");
    auto *joinBtn = new QPushButton("Join");
    joinBtn->setToolTip("Merge the selected meshes into the active one (Ctrl+J)");
    auto *sepBtn = new QPushButton("Separate");
    sepBtn->setToolTip("Split the active mesh into its loose parts (in Edit Mode: P separates the selected faces)");
    auto *delBtn = new QPushButton("Delete");
    for (QPushButton *b : {dupBtn, joinBtn, sepBtn, delBtn}) {
        b->setStyleSheet("padding:6px 4px;");
        b->setMinimumWidth(40);
        actRow->addWidget(b);
    }
    ol->addLayout(actRow);
    auto *originBtn = new QToolButton;
    originBtn->setText("Set Origin ▾");
    originBtn->setPopupMode(QToolButton::InstantPopup);
    ol->addWidget(originBtn);

    QVBoxLayout *tfl;
    ol->addWidget(box("TRANSFORM", &tfl));
    QHash<QString, QPushButton *> keyButtons;
    auto keyed = [&, m](QDoubleSpinBox *spin, const QString &property) {
        auto *wrap = new QWidget;
        auto *hl = new QHBoxLayout(wrap);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(3);
        auto *k = new QPushButton("◆");
        k->setFixedWidth(24);
        k->setToolTip("Insert/update a keyframe for " + property);
        k->setCheckable(true);
        hl->addWidget(spin, 1);
        hl->addWidget(k);
        keyButtons.insert(property, k);
        connect(k, &QPushButton::clicked, this, [m, property](bool) {
            if (!m->active()) return;
            m->keyProperty(m->activeId, property);
        });
        return wrap;
    };
    auto *name = new QLineEdit;
    auto *visible = new QCheckBox("Visible");
    auto *lx = dspin(-1000, 1000, 0.1, 3), *ly = dspin(-1000, 1000, 0.1, 3), *lz = dspin(-1000, 1000, 0.1, 3);
    auto *rx = dspin(-3600, 3600, 5, 1, "°"), *ry = dspin(-3600, 3600, 5, 1, "°"), *rz = dspin(-3600, 3600, 5, 1, "°");
    auto *sx = dspin(-1000, 1000, 0.1, 3), *sy = dspin(-1000, 1000, 0.1, 3), *sz = dspin(-1000, 1000, 0.1, 3);
    auto *opa = dspin(0, 100, 5, 0, " %");
    auto *tform = new QFormLayout;
    tform->setContentsMargins(0, 0, 0, 0);
    tform->addRow("Name", name);
    tform->addRow("Position X", keyed(lx, "position.x"));
    tform->addRow("Position Y", keyed(ly, "position.y"));
    tform->addRow("Position Z", keyed(lz, "position.z"));
    tform->addRow("Rotation X", keyed(rx, "rotation.x"));
    tform->addRow("Rotation Y", keyed(ry, "rotation.y"));
    tform->addRow("Rotation Z", keyed(rz, "rotation.z"));
    tform->addRow("Scale X", keyed(sx, "scale.x"));
    tform->addRow("Scale Y", keyed(sy, "scale.y"));
    tform->addRow("Scale Z", keyed(sz, "scale.z"));
    tform->addRow("Opacity", keyed(opa, "opacity"));
    tform->addRow(visible);
    tfl->addLayout(tform);
    auto *keyHint = new QLabel("Keys are explicit. Click ◆ beside a property to key it. Auto Key is OFF by default.");
    keyHint->setWordWrap(true);
    keyHint->setStyleSheet("color:palette(placeholder-text); font-size:10px;");
    tfl->addWidget(keyHint);
    auto *advTransform = new QToolButton;
    advTransform->setText("▸  Advanced Position & Layout");
    advTransform->setCheckable(true);
    advTransform->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto *advPanel = new QWidget;
    auto *advForm = new QFormLayout(advPanel);
    advForm->setContentsMargins(12, 2, 0, 2);
    auto *insetL = dspin(-10000,10000,1,0," px");
    auto *insetR = dspin(-10000,10000,1,0," px");
    auto *insetT = dspin(-10000,10000,1,0," px");
    auto *insetB = dspin(-10000,10000,1,0," px");
    advForm->addRow("Left", insetL);
    advForm->addRow("Right", insetR);
    advForm->addRow("Top", insetT);
    advForm->addRow("Bottom", insetB);
    advPanel->setVisible(false);
    connect(advTransform, &QToolButton::toggled, advPanel, [advPanel, advTransform](bool on) { advPanel->setVisible(on); advTransform->setText(on ? "▾  Advanced Position & Layout" : "▸  Advanced Position & Layout"); });
    tfl->addWidget(advTransform);
    tfl->addWidget(advPanel);

    QVBoxLayout *txl;
    auto *textBox = box("TEXT", &txl);
    auto *textEdit = new QPlainTextEdit;
    textEdit->setFixedHeight(60);
    auto *fontPx = new FrameInput;
    fontPx->setRange(8, 800);
    fontPx->setSuffix(" px");
    auto *tw = dspin(0.1, 100, 0.25, 2), *th = dspin(0.1, 100, 0.25, 2);
    auto *tform2 = new QFormLayout;
    tform2->setContentsMargins(0, 0, 0, 0);
    tform2->addRow(textEdit);
    tform2->addRow("Font size", fontPx);
    tform2->addRow("Plane W × H", rowOf({tw, th}));
    txl->addLayout(tform2);
    ol->addWidget(textBox);

    QVBoxLayout *iml;
    auto *imageBox = box("IMAGE", &iml);
    auto *imgPath = new QLineEdit;
    auto *imgBrowse = new QPushButton("Browse...");
    auto *iw = dspin(0.1, 100, 0.25, 2), *ih = dspin(0.1, 100, 0.25, 2);
    auto *iform = new QFormLayout;
    iform->setContentsMargins(0, 0, 0, 0);
    iform->addRow(rowOf({imgPath, imgBrowse}));
    iform->addRow("Plane W × H", rowOf({iw, ih}));
    iml->addLayout(iform);
    ol->addWidget(imageBox);
    ol->addStretch();
    objPage->setParent(this);
    objPage->hide();
    stack->addWidget(scrollPage(new PropertyInspector(m, lib)));

    // ================================================================ MODIFIERS tab
    auto *modPage = new QWidget;
    auto *ml = new QVBoxLayout(modPage);
    ml->setContentsMargins(4, 4, 8, 8);
    ml->setSpacing(8);
    auto *addMod = new QToolButton;
    addMod->setText("＋  Add Modifier ▾");
    addMod->setPopupMode(QToolButton::InstantPopup);
    ml->addWidget(addMod);
    auto *modHost = new QWidget;
    auto *modLay = new QVBoxLayout(modHost);
    modLay->setContentsMargins(0, 0, 0, 0);
    modLay->setSpacing(8);
    ml->addWidget(modHost);
    QVBoxLayout *phl;
    auto *physBox = box("PHYSICS  ·  RIGID BODY", &phl);
    auto *physOn = new QCheckBox("Enable rigid body");
    auto *physType = new QComboBox;
    physType->addItems({"Active (falls and bounces)", "Passive (stands in the way)"});
    auto *mass = dspin(0.01, 10000, 0.5, 2, " kg"), *fric = dspin(0, 1, 0.05, 2), *bounce = dspin(0, 1, 0.05, 2);
    auto *pform = new QFormLayout;
    pform->setContentsMargins(0, 0, 0, 0);
    pform->addRow(physOn);
    pform->addRow("Type", physType);
    pform->addRow("Mass", mass);
    pform->addRow("Friction", fric);
    pform->addRow("Bounciness", bounce);
    auto *planarPhysics=new QCheckBox("Constrain to canvas plane");planarPhysics->setObjectName("physicsPlanar");planarPhysics->setToolTip("Keep depth fixed while allowing movement and rotation in the canvas plane.");pform->addRow(planarPhysics);
    auto *collisionMargin=dspin(.0001,.2,.001,4," m"),*surfaceThickness=dspin(.002,10,.01,3," m");collisionMargin->setObjectName("physicsMargin");surfaceThickness->setObjectName("physicsThickness");pform->addRow("Contact margin",collisionMargin);pform->addRow("Flat surface depth",surfaceThickness);
    phl->addLayout(pform);
    auto *physHint = new QLabel("Gravity and the ground are set in the World tab. Press play to see bodies fall.");
    physHint->setWordWrap(true);
    physHint->setStyleSheet("color:palette(placeholder-text); font-size:10px;");
    phl->addWidget(physHint);
    ml->addWidget(physBox);
    ml->addStretch();
    stack->addWidget(scrollPage(modPage));

    // ================================================================ MATERIAL tab
    auto *matPage = new QWidget;
    auto *tl2 = new QVBoxLayout(matPage);
    tl2->setContentsMargins(4, 4, 8, 8);
    tl2->setSpacing(8);
    tl2->addWidget(TextureLibrary::createPanel(m,lib));
    auto *noObj = new QLabel("Select an object to edit its material.");
    noObj->setStyleSheet("color:palette(placeholder-text);");
    tl2->addWidget(noObj);
    QVBoxLayout *mtl;
    auto *matBox = box("SURFACE", &mtl);
    auto *matColor = new QPushButton;
    auto *alpha = dspin(0, 100, 5, 0, " %"), *rough = dspin(0, 1, 0.05, 2), *emis = dspin(0, 5, 0.1, 2);
    auto *flat = new QCheckBox("Flat colour (ignore lights)");
    auto *mform = new QFormLayout;
    mform->setContentsMargins(0, 0, 0, 0);
    mform->addRow("Colour", matColor);
    mform->addRow("Alpha", alpha);
    mform->addRow("Roughness", rough);
    mform->addRow("Emission", emis);
    mform->addRow(flat);
    mtl->addLayout(mform);
    tl2->addWidget(matBox);
    QVBoxLayout *ltl;
    auto *lightBox = box("LIGHT", &ltl);
    auto *lKind = new QComboBox;
    lKind->addItems({"Point", "Sun", "Spot", "Area"});
    auto *lColor = new QPushButton;
    auto *lEnergy = dspin(0, 100000, 50, 1), *lSpot = dspin(1, 179, 5, 0, "°"), *lBlend = dspin(0, 1, 0.05, 2), *lRad = dspin(0, 100, 0.05, 2, " m");
    auto *lform = new QFormLayout;
    lform->setContentsMargins(0, 0, 0, 0);
    lform->addRow("Type", lKind);
    lform->addRow("Colour", lColor);
    lform->addRow("Power", lEnergy);
    lform->addRow("Spot size", lSpot);
    lform->addRow("Spot blend", lBlend);
    lform->addRow("Radius", lRad);
    ltl->addLayout(lform);
    tl2->addWidget(lightBox);
    QVBoxLayout *ctl;
    auto *camBox = box("CAMERA  ·  what gets broadcast", &ctl);
    auto *cOrtho = new QCheckBox("Orthographic (flat, no perspective)");
    auto *cFov = dspin(5, 170, 1, 1, "°"), *cScale = dspin(0.5, 500, 0.5, 1, " m");
    auto *cform = new QFormLayout;
    cform->setContentsMargins(0, 0, 0, 0);
    cform->addRow(cOrtho);
    cform->addRow("Field of view", cFov);
    cform->addRow("Ortho width", cScale);
    ctl->addLayout(cform);
    tl2->addWidget(camBox);
    tl2->addStretch();
    stack->addWidget(scrollPage(matPage));

    // ================================================================ WORLD tab
    auto *worldPage = new QWidget;
    auto *wl = new QVBoxLayout(worldPage);
    wl->setContentsMargins(4, 4, 8, 8);
    wl->setSpacing(8);
    QVBoxLayout *wbl;
    auto *worldBox = box("BACKGROUND", &wbl);
    auto *addAudio=new QPushButton(uiIcon("audio"),QString());addAudio->setFixedSize(24,22);addAudio->setToolTip("Add background audio tracks");wbl->addWidget(addAudio,0,Qt::AlignRight);
    connect(addAudio,&QPushButton::clicked,this,[this]{for(const auto &path:QFileDialog::getOpenFileNames(this,"Add background audio",{},"Audio (*.wav *.mp3 *.aac *.m4a *.flac *.ogg)"))importFile(path);});
    auto *audioTracks=new QListWidget;audioTracks->setMaximumHeight(110);audioTracks->setToolTip("Select an audio track to edit its mix, timing and fades in the inspector");wbl->addWidget(audioTracks);
    connect(m,&BuildModel::changed,audioTracks,[=]{QSignalBlocker block(audioTracks);audioTracks->clear();for(const auto &o:m->design.objects)if(o.type=="Audio"){auto *item=new QListWidgetItem(uiIcon("audio"),o.name,audioTracks);item->setData(Qt::UserRole,o.id);}audioTracks->setVisible(audioTracks->count()>0);});
    connect(audioTracks,&QListWidget::itemClicked,this,[=](QListWidgetItem *item){m->select(item->data(Qt::UserRole).toString());tabBtns[0]->click();});
    auto *wTransparent = new QCheckBox("Transparent (film): lay the scene over the video");
    auto *wColor = new QPushButton;
    auto *wGrad = new QCheckBox("Sky gradient");
    auto *wHor = new QPushButton, *wZen = new QPushButton;
    auto *wform = new QFormLayout;
    wform->setContentsMargins(0, 0, 0, 0);
    wform->addRow(wTransparent);
    wform->addRow("Colour", wColor);
    wform->addRow(wGrad);
    wform->addRow("Horizon", wHor);
    wform->addRow("Zenith", wZen);
    wbl->addLayout(wform);
    wl->addWidget(worldBox);
    QVBoxLayout *wal;
    auto *ambBox = box("LIGHTING & MIST", &wal);
    auto *wAmb = dspin(0, 5, 0.05, 2), *wFog = dspin(0, 2, 0.01, 3);
    auto *wAmbColor = new QPushButton, *wFogColor = new QPushButton;
    auto *aform = new QFormLayout;
    aform->setContentsMargins(0, 0, 0, 0);
    aform->addRow("Ambient strength", wAmb);
    aform->addRow("Ambient colour", wAmbColor);
    aform->addRow("Mist density", wFog);
    aform->addRow("Mist colour", wFogColor);
    wal->addLayout(aform);
    wl->addWidget(ambBox);
    QVBoxLayout *wpl;
    auto *worldPhys = box("PHYSICS", &wpl);
    auto *wGrav = dspin(0, 100, 0.5, 2, " m/s²");
    auto *wFloor = new QCheckBox("Ground plane at z = 0");
    auto *gform = new QFormLayout;
    gform->setContentsMargins(0, 0, 0, 0);
    gform->addRow("Gravity", wGrav);
    gform->addRow(wFloor);
    wpl->addLayout(gform);
    wl->addWidget(worldPhys);
    QVBoxLayout *wvl;
    auto *viewBox = box("VIEWPORT OVERLAYS", &wvl);
    auto *oGrid = new QCheckBox("Composition grid"), *oAxes = new QCheckBox("Center guides"), *oGiz = new QCheckBox("Lights and camera");
    oGrid->setChecked(false);
    oAxes->setChecked(true);
    oGiz->setChecked(true);
    wvl->addWidget(oGrid);
    wvl->addWidget(oAxes);
    wvl->addWidget(oGiz);
    wl->addWidget(viewBox);
    auto *dispHint = new QLabel("Resolution, aspect ratio, frame rate, motion blur, colour management and render region are in the Display tab of the Studio.");
    dispHint->setWordWrap(true);
    dispHint->setStyleSheet("color:palette(placeholder-text); font-size:10px;");
    wl->addWidget(dispHint);
    wl->addStretch();
    stack->addWidget(scrollPage(worldPage));
    stack->addWidget(scrollPage(animationCueEditor(m)));
    stack->addWidget(scrollPage(sceneEffectEditor(m)));

    // ================================================================ menus
    auto *addMenu = new QMenu(this);
    {
        QMenu *shapes = addMenu->addMenu("2D Shapes");
        for (const QString &kind : {"Rectangle", "Ellipse"}) shapes->addAction(kind, this, [m, kind] {
            m->pushUndo();
            DObject o; o.id = Library::newId(); o.name = m->uniqueName(kind); o.type = "Shape";
            o.w = 4; o.h = 2; o.base.rot = {90,0,0}; o.mat.color = Qt::white; o.mat.shadeless = true;
            o.style["shape"] = kind;o.base.loc=m->design.cursor;o.captureDefaults(); m->design.objects.append(o); m->select(o.id); m->notify();
        });
        QMenu *mesh = addMenu->addMenu("3D Mesh");
        for (const QString &k : Mesh::primitiveKinds()) ((k=="Plane"||k=="Circle")?shapes:mesh)->addAction(k, this, [m, k] { m->addMesh(k); });
        addMenu->addAction("Text", this, [m] { m->addText(); });
        auto *media=addMenu->addMenu(uiIcon("image"),"Media");
        media->addAction(uiIcon("folder"),"Composition media…",this,[this]{createDesignMediaDialog(m_model,this)->open();});
        media->addSeparator();
        media->addAction(uiIcon("image"),"Image or SVG…",this,[this]{const auto path=QFileDialog::getOpenFileName(this,"Add image",{},"Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.svg)");if(!path.isEmpty())importFile(path);});
        media->addAction(uiIcon("play"),"Video…",this,[this]{const auto path=QFileDialog::getOpenFileName(this,"Add video",{},"Videos (*.mp4 *.mov *.mkv *.avi *.webm)");if(!path.isEmpty())importFile(path);});
        media->addAction(uiIcon("audio"),"Audio…",this,[this]{const auto paths=QFileDialog::getOpenFileNames(this,"Add audio",{},"Audio (*.wav *.mp3 *.aac *.m4a *.flac *.ogg)");for(const auto &path:paths)importFile(path);});
        QMenu *light = addMenu->addMenu("Light");
        for (const QString &k : {"Point", "Sun", "Spot", "Area"}) light->addAction(k, this, [m, k] { m->addLight(k); });
    }
    addBtn->setMenu(addMenu);
    connect(canvas, &BuildCanvas::addMenuRequested, this, [addMenu](const QPoint &g) { addMenu->exec(g); });

    auto *originMenu = new QMenu(this);
    originMenu->addAction("Origin to Geometry", this, [this, m] { if (!m->setOrigin("Geometry")) say("Select a mesh first", true); });
    originMenu->addAction("Origin to Cursor",this,[m]{m->setOrigin("Cursor");});
    auto *boundsOrigin=originMenu->addMenu("Position");for(const auto &name:QStringList{"Top Left","Top Center","Top Right","Center Left","Center","Center Right","Bottom Left","Bottom Center","Bottom Right"})boundsOrigin->addAction(name,this,[m,name]{m->setOrigin("Bounds:"+name);});
    originMenu->addAction("Origin to Bottom", this, [this, m] { if (!m->setOrigin("Bottom")) say("Select a mesh first", true); });
    originMenu->addAction("Origin to World Origin", this, [this, m] { if (!m->setOrigin("World")) say("Select a mesh first", true); });
    originMenu->addAction("Origin to Selected (Edit Mode)", this, [this, m] { if (!m->setOrigin("Selected")) say("In Edit Mode, select vertices first", true); });
    originBtn->setMenu(originMenu);

    auto *objMenu = new QMenu(this);
    objMenu->addMenu(originMenu)->setText("Set Origin");
    objMenu->addAction("Join   Ctrl+J", this, [this, m] { say(m->joinSelected() ? "Joined" : "Select two or more mesh objects", false); });
    objMenu->addAction("Separate by Loose Parts", this, [this, m] { say(m->separateLooseParts() ? "Separated" : "Nothing to separate", false); });
    objMenu->addAction("Duplicate   Shift+D", this, [m, canvas] { if (!m->duplicateSelected().isEmpty()) canvas->startMove(); });
    objMenu->addAction("Delete   X", this, [m] { m->deleteSelected(); });
    objMenu->addSeparator();
    objMenu->addAction("Hide   H", this, [m] { m->hideSelected(true); });
    objMenu->addAction("Show All   Alt+H", this, [m] { m->showAll(); });
    objBtn->setMenu(objMenu);

    auto *meshMenu = new QMenu(this);
    meshMenu->addAction("Extrude   E", this, [canvas] { canvas->startExtrude(); });
    meshMenu->addAction("Inset   I", this, [canvas] { canvas->startInset(); });
    meshMenu->addAction("Loop Cut   Ctrl+R", this, [canvas] { canvas->startLoopCut(); });
    meshMenu->addAction("Subdivide   W", this, [this, m] { say(m->opSubdivide() ? "Subdivided" : "Select edges or faces", false); });
    meshMenu->addAction("Fill   F", this, [this, m] { say(m->opFill() ? "Filled" : "Select 3 or more vertices", false); });
    meshMenu->addAction("Flatten   Shift+F", this, [this, m] { say(m->opFlatten() ? "Flattened" : "Select 3 or more vertices", false); });
    meshMenu->addAction("Merge at Center   Alt+M", this, [this, m] { say(m->opMerge() ? "Merged" : "Select 2 or more vertices", false); });
    meshMenu->addAction("Flip Normals   Alt+N", this, [m] { m->opFlip(); });
    meshMenu->addSeparator();
    meshMenu->addAction("Separate Selection   P", this, [this, m] { say(m->opSeparate() ? "Separated into a new object" : "Select faces to separate", false); });
    meshMenu->addAction("Split   Y", this, [this, m] { say(m->opSplit() ? "Split" : "Select faces to split", false); });
    meshMenu->addAction("Delete   X", this, [m] { m->opDelete(); });
    meshMenu->addSeparator();
    meshMenu->addAction("Select All   A", this, [m] { m->editSelectAll(true); });
    meshMenu->addAction("Select None   Alt+A", this, [m] { m->editSelectAll(false); });
    meshMenu->addAction("Invert   Ctrl+I", this, [m] { m->editInvert(); });
    meshBtn->setMenu(meshMenu);

    auto *modMenu = new QMenu(this);
    for (const QString &k : {"Mirror", "Array", "Solidify", "Bevel", "Boolean"})
        modMenu->addAction(k, this, [this, m, k] {
            DObject *o = m->active();
            if (!o || o->type != "Mesh") {
                say("Select a mesh object first", true);
                return;
            }
            m->pushUndo();
            Modifier md;
            md.type = k;
            o->mods.append(md);
            m->notify();
        });
    addMod->setMenu(modMenu);

    // ================================================================ keeping the panels in step
    auto listSig = std::make_shared<QString>();
    auto modSig = std::make_shared<QString>();
    auto refreshList = [=, this] {
        if (*renaming) return;
        QString sig;
        for (const DObject &o : m->design.objects) sig += o.id + o.name + (o.visible ? "1" : "0") + (m->sel.contains(o.id) ? "s" : "") + (o.id == m->activeId ? "a" : "") + ";";
        if (sig == *listSig) return;
        *listSig = sig;
        QSignalBlocker b(list);
        list->clear();
        for (const DObject &o : m->design.objects) {
            const QString icon = o.type == "Light" ? "☀   " : o.type == "Camera" ? "🎥   " : o.type == "Text" ? "T   " : o.type == "Image" ? "▨   " : "▣   ";
            auto *it = new QListWidgetItem(icon + o.name);
            it->setData(Qt::UserRole, o.id);
            it->setData(Qt::EditRole, o.name);
            it->setData(Qt::DisplayRole, icon + o.name);
            it->setFlags(it->flags() | Qt::ItemIsEditable);
            if (!o.visible) it->setForeground(QColor("#64748b"));
            list->addItem(it);
            if (m->sel.contains(o.id)) it->setSelected(true);
            if (o.id == m->activeId) list->setCurrentItem(it, QItemSelectionModel::NoUpdate);
        }
    };

    auto syncObject = [=, this] {
        *syncing = true;
        DObject *o = m->active();
        const bool has = o != nullptr;
        for (QWidget *w : {static_cast<QWidget *>(name), static_cast<QWidget *>(lx), static_cast<QWidget *>(ly), static_cast<QWidget *>(lz),
                           static_cast<QWidget *>(rx), static_cast<QWidget *>(ry), static_cast<QWidget *>(rz), static_cast<QWidget *>(sx),
                           static_cast<QWidget *>(sy), static_cast<QWidget *>(sz), static_cast<QWidget *>(opa), static_cast<QWidget *>(visible)})
            w->setEnabled(has);
        if (o) {
            const DProps p = m->design.propsAt(*o, m->t);
            if (!name->hasFocus()) name->setText(o->name);
            lx->setValue(p.loc.x()); ly->setValue(p.loc.y()); lz->setValue(p.loc.z());
            rx->setValue(p.rot.x()); ry->setValue(p.rot.y()); rz->setValue(p.rot.z());
            sx->setValue(p.scl.x()); sy->setValue(p.scl.y()); sz->setValue(p.scl.z());
            opa->setValue(p.opacity * 100);
            visible->setChecked(o->visible);
            insetL->setValue(o->insetLeft); insetR->setValue(o->insetRight);
            insetT->setValue(o->insetTop); insetB->setValue(o->insetBottom);
            for (auto it = keyButtons.begin(); it != keyButtons.end(); ++it) {
                QSignalBlocker kb(it.value());
                it.value()->setChecked(m->propertyIsKeyed(*o, it.key(), m->t));
            }
            const bool isText = o->type == "Text", isImage = o->type == "Image";
            textBox->setVisible(isText);
            imageBox->setVisible(isImage);
            if (isText) {
                if (!textEdit->hasFocus()) textEdit->setPlainText(o->text);
                fontPx->setValue(o->fontPx);
                tw->setValue(o->w);
                th->setValue(o->h);
            }
            if (isImage) {
                if (!imgPath->hasFocus()) imgPath->setText(o->image);
                iw->setValue(o->w);
                ih->setValue(o->h);
            }
        } else {
            textBox->hide();
            imageBox->hide();
        }
        const bool mesh = o && o->type == "Mesh";
        joinBtn->setEnabled(mesh);
        sepBtn->setEnabled(mesh);
        originBtn->setEnabled(mesh);
        delBtn->setEnabled(o && o->type != "Camera");
        dupBtn->setEnabled(o && o->type != "Camera");
        *syncing = false;
    };

    auto syncMaterial = [=, this] {
        *syncing = true;
        DObject *o = m->active();
        noObj->setVisible(!o);
        const bool surf = o && (o->type == "Mesh" || o->type == "Text" || o->type == "Image");
        matBox->setVisible(surf);
        lightBox->setVisible(o && o->type == "Light");
        camBox->setVisible(o && o->type == "Camera");
        if (surf) {
            paintColorButton(matColor, o->mat.color);
            alpha->setValue(o->mat.alpha * 100);
            rough->setValue(o->mat.roughness);
            emis->setValue(o->mat.emission);
            flat->setChecked(o->mat.shadeless);
            matColor->setToolTip(o->type == "Image" ? "Images keep their own colours" : QString());
            matColor->setEnabled(o->type != "Image");
            rough->setEnabled(!o->mat.shadeless);
            emis->setEnabled(!o->mat.shadeless);
        }
        if (o && o->type == "Light") {
            lKind->setCurrentText(o->light.kind);
            paintColorButton(lColor, o->light.color);
            lEnergy->setValue(o->light.energy);
            lSpot->setValue(o->light.spotSize);
            lBlend->setValue(o->light.spotBlend);
            lRad->setValue(o->light.radius);
            lSpot->setEnabled(o->light.kind == "Spot");
            lBlend->setEnabled(o->light.kind == "Spot");
            lEnergy->setSuffix(o->light.kind == "Sun" ? " W/m²" : " W");
        }
        if (o && o->type == "Camera") {
            cOrtho->setChecked(o->cam.ortho);
            cFov->setValue(o->cam.fov);
            cScale->setValue(o->cam.orthoScale);
            cFov->setEnabled(!o->cam.ortho);
            cScale->setEnabled(o->cam.ortho);
        }
        *syncing = false;
    };

    auto syncWorld = [=, this] {
        *syncing = true;
        const World &w = m->design.world;
        wTransparent->setChecked(w.transparent);
        paintColorButton(wColor, w.color);
        wColor->setEnabled(!w.transparent && !w.gradient);
        wGrad->setChecked(w.gradient);
        wGrad->setEnabled(!w.transparent);
        paintColorButton(wHor, w.horizon);
        paintColorButton(wZen, w.zenith);
        wHor->setEnabled(!w.transparent && w.gradient);
        wZen->setEnabled(!w.transparent && w.gradient);
        wAmb->setValue(w.ambient);
        paintColorButton(wAmbColor, w.ambientColor);
        wFog->setValue(w.fog);
        paintColorButton(wFogColor, w.fogColor);
        wGrav->setValue(w.gravity);
        wFloor->setChecked(w.floor);
        *syncing = false;
    };

    // modifiers + physics are rebuilt when the structure changes, and just refreshed otherwise
    auto syncPhysics = [=, this] {
        *syncing = true;
        DObject *o = m->active();
        physBox->setEnabled(o && o->type != "Light" && o->type != "Camera");
        if (o) {
            physOn->setChecked(o->phys.enabled);
            physType->setCurrentIndex(o->phys.active ? 0 : 1);
            mass->setValue(o->phys.mass);
            fric->setValue(o->phys.friction);
            bounce->setValue(o->phys.bounce);
            planarPhysics->setChecked(o->style["physics.planar"].toBool(o->type!="Mesh"));planarPhysics->setEnabled(o->phys.enabled&&o->phys.active);
            collisionMargin->setValue(o->style["physics.margin"].toDouble(.005));surfaceThickness->setValue(o->style["physics.thickness"].toDouble(.04));collisionMargin->setEnabled(o->phys.enabled);surfaceThickness->setEnabled(o->phys.enabled);
            mass->setEnabled(o->phys.enabled&&o->phys.active);
            fric->setEnabled(o->phys.enabled);
            bounce->setEnabled(o->phys.enabled);
            physType->setEnabled(o->phys.enabled);
        }
        *syncing = false;
    };

    std::function<void()> rebuildMods = [=, this] {
        DObject *o = m->active();
        QString sig = o ? o->id : QString();
        sig+=QString("|edit:%1|locked:%2").arg(m->editMode).arg(o&&o->locked);
        if (o)
            for (const Modifier &md : o->mods) sig += "|" + md.type;
        if (o)
            for (const DObject &x : m->design.objects) sig += x.id + x.name;
        if (sig == *modSig) return;
        *modSig = sig;
        while (QLayoutItem *it = modLay->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
        addMod->setEnabled(o && o->type == "Mesh");
        if (!o || o->type != "Mesh") {
            auto *none = new QLabel(o ? "Modifiers work on mesh objects." : "Select a mesh object to add modifiers.");
            none->setStyleSheet("color:palette(placeholder-text);");
            modLay->addWidget(none);
            return;
        }
        const QString oid = o->id;
        for (int i = 0; i < o->mods.size(); ++i) {
            const Modifier &md = o->mods[i];
            QVBoxLayout *inner;
            auto *card = box(md.type.toUpper(), &inner);
            auto *head = new QHBoxLayout;
            auto *on = new QCheckBox("Enabled");
            on->setChecked(md.on);
            auto *up = new QPushButton("↑"), *dn = new QPushButton("↓"), *rm = new QPushButton("✕");
            for (QPushButton *b : {up, dn, rm}) b->setFixedWidth(30);
            head->addWidget(on, 1);
            auto *apply=new QPushButton(uiIcon("apply"),"");apply->setObjectName("applyModifier");apply->setFixedWidth(26);apply->setToolTip(i==0?"Apply modifier · Ctrl+Shift+M":"Apply this modifier and the preceding stack to the mesh");apply->setEnabled(!m->editMode&&!o->locked);head->addWidget(apply);
            connect(apply,&QPushButton::clicked,this,[=,this]{say(m->applyModifier(oid,i)?"Modifier applied to mesh":"Exit Edit Mode and select an unlocked mesh",false);});
            head->addWidget(up);
            head->addWidget(dn);
            head->addWidget(rm);
            inner->addLayout(head);
            auto *form = new QFormLayout;
            form->setContentsMargins(0, 0, 0, 0);
            auto mod = [=, this]() -> Modifier * {
                DObject *ob = m->byId(oid);
                return ob && i < ob->mods.size() ? &ob->mods[i] : nullptr;
            };
            connect(on, &QCheckBox::toggled, this, [=, this](bool v) { if (Modifier *x = mod()) { touch("mod"); x->on = v; m->notify(); } });
            connect(rm, &QPushButton::clicked, this, [=, this] { if (DObject *ob = m->byId(oid)) { m->pushUndo(); ob->mods.removeAt(i); m->notify(); } });
            connect(up, &QPushButton::clicked, this, [=, this] { if (DObject *ob = m->byId(oid); ob && i > 0) { m->pushUndo(); ob->mods.swapItemsAt(i, i - 1); m->notify(); } });
            connect(dn, &QPushButton::clicked, this, [=, this] { if (DObject *ob = m->byId(oid); ob && i + 1 < ob->mods.size()) { m->pushUndo(); ob->mods.swapItemsAt(i, i + 1); m->notify(); } });
            auto numField = [&](const QString &label, double lo, double hi, double step, int dec, double val, std::function<void(Modifier &, double)> set, const QString &suffix = QString()) {
                auto *s = dspin(lo, hi, step, dec, suffix);
                s->setValue(val);
                connect(s, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { if (*syncing) return; if (Modifier *x = mod()) { touch("mod" + label); set(*x, v); m->notify(); } });
                form->addRow(label, s);
                return s;
            };
            if (md.type == "Mirror") {
                auto *cx = new QCheckBox("X"), *cy = new QCheckBox("Y"), *cz = new QCheckBox("Z");
                cx->setChecked(md.ax); cy->setChecked(md.ay); cz->setChecked(md.az);
                connect(cx, &QCheckBox::toggled, this, [=, this](bool v) { if (Modifier *x = mod()) { touch("mir"); x->ax = v; m->notify(); } });
                connect(cy, &QCheckBox::toggled, this, [=, this](bool v) { if (Modifier *x = mod()) { touch("mir"); x->ay = v; m->notify(); } });
                connect(cz, &QCheckBox::toggled, this, [=, this](bool v) { if (Modifier *x = mod()) { touch("mir"); x->az = v; m->notify(); } });
                form->addRow("Axis", rowOf({cx, cy, cz}));
                for(const QString &kind:QStringList{"bisect","flip"}){
                    QList<QWidget*> controls;
                    for(const QString &axis:QStringList{"x","y","z"}){auto *toggle=new QCheckBox(axis.toUpper());const QString path=kind+"."+axis;toggle->setChecked(md.options[path].toBool());controls.append(toggle);connect(toggle,&QCheckBox::toggled,this,[=](bool value){if(auto *x=mod()){touch("mirror"+path);x->options[path]=value;m->notify();}});}
                    form->addRow(kind=="bisect"?"Bisect":"Flip",rowOf(controls));
                }
                for(const QString &axis:QStringList{"x","y","z"}){const QString path="offset."+axis;numField("Offset "+axis.toUpper(),-10000,10000,.01,3,md.options[path].toDouble(),[path](Modifier &x,double value){x.options[path]=value;});}
                auto *mirrorObject=new QComboBox;mirrorObject->addItem("Object origin",QString());
                for(const auto &object:m->design.objects)if(object.id!=oid)mirrorObject->addItem(object.name,object.id);
                mirrorObject->setCurrentIndex(qMax(0,mirrorObject->findData(md.target)));
                auto *pick=new QPushButton(uiIcon("picker"),"");pick->setObjectName("pickMirrorObject");pick->setFixedWidth(26);pick->setToolTip("Pick mirror reference in the viewport · Escape cancels");form->addRow("Mirror object",rowOf({mirrorObject,pick}));
                connect(pick,&QPushButton::clicked,this,[=,this]{QPointer<QComboBox> control=mirrorObject;canvas->pickSceneObject([=,this](QString target){if(!control)return;if(target==oid){say("Choose a different object as the mirror reference",true);return;}if(auto *x=mod();x&&m->byId(target)){m->pushUndo();x->target=target;control->setCurrentIndex(control->findData(target));m->notify();say("Mirror reference selected");}});});
                connect(mirrorObject,&QComboBox::activated,this,[=]{if(auto *x=mod()){touch("mirrorObject");x->target=mirrorObject->currentData().toString();m->notify();}});
                for(const QString &name:QStringList{"clipping","merge"}){auto *toggle=new QCheckBox(name=="clipping"?"Clipping":"Merge");toggle->setChecked(md.options[name].toBool(name=="merge"));form->addRow(toggle);connect(toggle,&QCheckBox::toggled,this,[=](bool value){if(auto *x=mod()){touch(name);x->options[name]=value;m->notify();}});}
                numField("Merge distance",.000001,10,.001,6,md.options["mergeDistance"].toDouble(.001),[](Modifier &x,double value){x.options["mergeDistance"]=value;});
            } else if (md.type == "Array") {
                auto *cnt = new FrameInput;
                cnt->setRange(1, 64);
                cnt->setValue(md.count);
                connect(cnt, &QSpinBox::valueChanged, this, [=, this](int v) { if (*syncing) return; if (Modifier *x = mod()) { touch("arrc"); x->count = v; m->notify(); } });
                form->addRow("Count", cnt);
                auto *r1 = dspin(-50, 50, 0.1, 2), *r2 = dspin(-50, 50, 0.1, 2), *r3 = dspin(-50, 50, 0.1, 2);
                r1->setValue(md.rel.x()); r2->setValue(md.rel.y()); r3->setValue(md.rel.z());
                auto relSet = [=, this](int axis) { return [=, this](double v) { if (*syncing) return; if (Modifier *x = mod()) { touch("arrr"); if (axis == 0) x->rel.setX(float(v)); else if (axis == 1) x->rel.setY(float(v)); else x->rel.setZ(float(v)); m->notify(); } }; };
                connect(r1, &QDoubleSpinBox::valueChanged, this, relSet(0));
                connect(r2, &QDoubleSpinBox::valueChanged, this, relSet(1));
                connect(r3, &QDoubleSpinBox::valueChanged, this, relSet(2));
                form->addRow("Relative", rowOf({r1, r2, r3}));
                auto *c1 = dspin(-100, 100, 0.1, 2), *c2 = dspin(-100, 100, 0.1, 2), *c3 = dspin(-100, 100, 0.1, 2);
                c1->setValue(md.cst.x()); c2->setValue(md.cst.y()); c3->setValue(md.cst.z());
                auto cstSet = [=, this](int axis) { return [=, this](double v) { if (*syncing) return; if (Modifier *x = mod()) { touch("arrk"); if (axis == 0) x->cst.setX(float(v)); else if (axis == 1) x->cst.setY(float(v)); else x->cst.setZ(float(v)); m->notify(); } }; };
                connect(c1, &QDoubleSpinBox::valueChanged, this, cstSet(0));
                connect(c2, &QDoubleSpinBox::valueChanged, this, cstSet(1));
                connect(c3, &QDoubleSpinBox::valueChanged, this, cstSet(2));
                form->addRow("Constant", rowOf({c1, c2, c3}));
            } else if (md.type == "Solidify") {
                numField("Thickness", -10, 10, 0.05, 3, md.thickness, [](Modifier &x, double v) { x.thickness = v; });
                numField("Offset", -1, 1, 0.1, 2, md.offset, [](Modifier &x, double v) { x.offset = v; });
            } else if (md.type == "Bevel") {
                auto choice=[&](const QString &label,const QString &path,const QStringList &values,const QString &fallback){auto *box=new QComboBox;box->addItems(values);box->setCurrentText(md.options[path].toString(fallback));form->addRow(label,box);connect(box,&QComboBox::activated,this,[=]{if(auto *x=mod()){touch("bevel"+path);x->options[path]=box->currentText();m->notify();}});};
                choice("Affect","affect",{"Edges","Vertices"},"Edges");choice("Width type","widthType",{"Offset","Width","Depth","Percent"},"Offset");
                numField("Amount",0,100,0.01,3,md.width,[](Modifier &x,double value){x.width=value;});
                numField("Segments",1,16,1,0,md.options["segments"].toInt(1),[](Modifier &x,double value){x.options["segments"]=qRound(value);});
                choice("Limit method","limit",{"None","Angle"},"None");
                numField("Angle",0,180,1,1,md.options["angle"].toDouble(30),[](Modifier &x,double value){x.options["angle"]=value;},"°");
                numField("Profile",0,1,.05,2,md.options["profile"].toDouble(.5),[](Modifier &x,double value){x.options["profile"]=value;});
                auto *clamp=new QCheckBox("Clamp overlap");clamp->setChecked(md.options["clamp"].toBool(true));form->addRow(clamp);connect(clamp,&QCheckBox::toggled,this,[=](bool value){if(auto *x=mod()){touch("bevelClamp");x->options["clamp"]=value;m->notify();}});
            } else if (md.type == "Boolean") {
                auto *op = new QComboBox;
                op->addItems({"Difference", "Union", "Intersect"});
                op->setCurrentText(md.op);
                connect(op, &QComboBox::currentTextChanged, this, [=, this](const QString &v) { if (*syncing) return; if (Modifier *x = mod()) { touch("boolop"); x->op = v; m->notify(); } });
                form->addRow("Operation", op);
                auto *tgt = new QComboBox;
                tgt->addItem("(choose an object)", QString());
                for (const DObject &x : m->design.objects)
                    if (x.type == "Mesh" && x.id != oid) tgt->addItem(x.name, x.id);
                tgt->setCurrentIndex(qMax(0, tgt->findData(md.target)));
                connect(tgt, &QComboBox::activated, this, [=, this](int idx) { if (Modifier *x = mod()) { touch("booltgt"); x->target = tgt->itemData(idx).toString(); m->notify(); } });
                form->addRow("Object", tgt);
                auto *hint = new QLabel("Hide the cutting object (select it, press H).");
                hint->setStyleSheet("color:palette(placeholder-text); font-size:10px;");
                form->addRow(hint);
            }
            inner->addLayout(form);
            modLay->addWidget(card);
        }
    };

    auto refreshAll = [=, this] {
        refreshList();
        syncObject();
        syncMaterial();
        syncPhysics();
        rebuildMods();
        syncWorld();
    };
    connect(m, &BuildModel::changed, this, refreshAll);
    connect(m, &BuildModel::selectionChanged, this, refreshAll);
    connect(m, &BuildModel::timeChanged, this, [=, this] { syncObject(); });

    auto refreshTime = [=, this] {
        timeLabel->setText(QString::number(m->t / m->frameMs()));
        timeLabel->setToolTip(clock(m->t)+" / "+clock(m->design.durationMs));
        play->setText({});play->setIcon(uiIcon(m->playing?"pause":"play"));
        DObject *o = m->active();
        QSignalBlocker b(interp);
        int k = o ? (m->selKey >= 0 ? m->selKey : o->keyIndexNear(m->t)) : -1;
        interp->setEnabled(o && k >= 0);
        if (o && !m->selChannel.isEmpty()) {
            const auto *track = o->track(m->selChannel);
            int index = track ? track->indexAt(m->t) : -1;
            interp->setEnabled(index >= 0);
            if (index >= 0) interp->setCurrentText(track->keys[index].interpolation);
        } else if (o && k >= 0 && k < o->keys.size()) interp->setCurrentText(o->keys[k].interp);
    };
    connect(m, &BuildModel::changed, this, refreshTime);
    connect(m, &BuildModel::timeChanged, this, refreshTime);
    connect(m, &BuildModel::selectionChanged, this, refreshTime);

    // ================================================================ edits from the panels
    connect(list, &QListWidget::itemSelectionChanged, this, [=, this] {
        if (*syncing) return;
        QSet<QString> ids;
        for (QListWidgetItem *it : list->selectedItems()) ids.insert(it->data(Qt::UserRole).toString());
        const QListWidgetItem *cur = list->currentItem();
        *syncing = true;
        m->setSelection(ids, cur ? cur->data(Qt::UserRole).toString() : QString());
        *syncing = false;
        refreshAll();
    });
    connect(list, &QListWidget::itemChanged, this, [=, this](QListWidgetItem *it) {
        // the row text carries an icon; the editor shows the bare name, so strip it back off
        QString text = it->text().trimmed();
        const DObject *o = m->byId(it->data(Qt::UserRole).toString());
        if (!o) return;
        for (const QString &icon : {"☀", "🎥", "T", "▨", "▣"})
            if (text.startsWith(icon + " ") && o->name.left(1) != icon) { text = text.mid(icon.size()).trimmed(); break; }
        *renaming = true;
        m->rename(o->id, text);
        *renaming = false;
        listSig->clear();
        QTimer::singleShot(0, this, refreshAll);
    });
    connect(dupBtn, &QPushButton::clicked, this, [m, canvas] { if (!m->duplicateSelected().isEmpty()) canvas->startMove(); });
    connect(delBtn, &QPushButton::clicked, this, [m] { m->deleteSelected(); });
    connect(joinBtn, &QPushButton::clicked, this, [this, m] { say(m->joinSelected() ? "Joined" : "Select two or more mesh objects; the active one stays", false); });
    connect(sepBtn, &QPushButton::clicked, this, [this, m] { say(m->separateLooseParts() ? "Separated by loose parts" : "This mesh is one piece. In Edit Mode, select faces and press P", false); });

    connect(name, &QLineEdit::editingFinished, this, [=, this] { if (*syncing) return; if (DObject *o = m->active()) m->rename(o->id, name->text()); });
    connect(visible, &QCheckBox::toggled, this, [=, this](bool v) { if (*syncing) return; if (DObject *o = m->active()) { touch("vis"); o->visible = v; m->notify(); } });
    auto propEdit = [=, this](QDoubleSpinBox *s, std::function<void(DProps &, double)> f) {
        connect(s, &QDoubleSpinBox::valueChanged, this, [=, this](double v) {
            if (*syncing || !m->active()) return;
            touch("props");
            m->editProps(m->activeId, [&](DProps &p) { f(p, v); });
        });
    };
    propEdit(lx, [](DProps &p, double v) { p.loc.setX(float(v)); });
    propEdit(ly, [](DProps &p, double v) { p.loc.setY(float(v)); });
    propEdit(lz, [](DProps &p, double v) { p.loc.setZ(float(v)); });
    propEdit(rx, [](DProps &p, double v) { p.rot.setX(float(v)); });
    propEdit(ry, [](DProps &p, double v) { p.rot.setY(float(v)); });
    propEdit(rz, [](DProps &p, double v) { p.rot.setZ(float(v)); });
    propEdit(sx, [](DProps &p, double v) { p.scl.setX(float(v)); });
    propEdit(sy, [](DProps &p, double v) { p.scl.setY(float(v)); });
    propEdit(sz, [](DProps &p, double v) { p.scl.setZ(float(v)); });
    propEdit(opa, [](DProps &p, double v) { p.opacity = v / 100.0; });
    auto layoutEdit = [=, this](QDoubleSpinBox *s, const QString &tag, std::function<void(DObject &, double)> f) {
        connect(s, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { if (*syncing || !m->active()) return; touch(tag); f(*m->active(), v); m->notify(); });
    };
    layoutEdit(insetL, "insetL", [](DObject &o,double v){o.insetLeft=v;});
    layoutEdit(insetR, "insetR", [](DObject &o,double v){o.insetRight=v;});
    layoutEdit(insetT, "insetT", [](DObject &o,double v){o.insetTop=v;});
    layoutEdit(insetB, "insetB", [](DObject &o,double v){o.insetBottom=v;});

    auto objEdit = [=, this](const QString &tag, std::function<void(DObject &)> f) {
        if (*syncing) return;
        if (DObject *o = m->active()) { touch(tag); f(*o); m->notify(); }
    };
    connect(textEdit, &QPlainTextEdit::textChanged, this, [=, this] { objEdit("text", [&](DObject &o) { o.text = textEdit->toPlainText(); }); });
    connect(fontPx, &QSpinBox::valueChanged, this, [=, this](int v) { objEdit("font", [&](DObject &o) { o.fontPx = v; }); });
    connect(tw, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("tw", [&](DObject &o) { o.w = v; }); });
    connect(th, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("th", [&](DObject &o) { o.h = v; }); });
    connect(iw, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("iw", [&](DObject &o) { o.w = v; }); });
    connect(ih, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("ih", [&](DObject &o) { o.h = v; }); });
    connect(imgPath, &QLineEdit::editingFinished, this, [=, this] { objEdit("img", [&](DObject &o) { o.image = imgPath->text(); }); });
    connect(imgBrowse, &QPushButton::clicked, this, [=, this] {
        const QString path = QFileDialog::getOpenFileName(this, "Choose a picture", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif);;All files (*)");
        if (path.isEmpty()) return;
        objEdit("img", [&](DObject &o) {
            o.image = path;
            const QImage im(path);
            if (!im.isNull()) o.h = o.w * im.height() / qMax(1, im.width());
        });
    });

    auto pickColor = [=, this](QPushButton *btn, std::function<QColor *()> get) {
        connect(btn, &QPushButton::clicked, this, [=, this] {
            QColor *c = get();
            if (!c) return;
            const QColor n = ColorPicker::getColor(*c, this, "Colour");
            if (!n.isValid()) return;
            m->pushUndo();
            *c = n;
            m->notify();
        });
    };
    pickColor(matColor, [=, this]() -> QColor * { DObject *o = m->active(); return o ? &o->mat.color : nullptr; });
    pickColor(lColor, [=, this]() -> QColor * { DObject *o = m->active(); return o ? &o->light.color : nullptr; });
    pickColor(wColor, [=, this]() -> QColor * { return &m->design.world.color; });
    pickColor(wHor, [=, this]() -> QColor * { return &m->design.world.horizon; });
    pickColor(wZen, [=, this]() -> QColor * { return &m->design.world.zenith; });
    pickColor(wAmbColor, [=, this]() -> QColor * { return &m->design.world.ambientColor; });
    pickColor(wFogColor, [=, this]() -> QColor * { return &m->design.world.fogColor; });

    connect(alpha, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("alpha", [&](DObject &o) { o.mat.alpha = v / 100.0; }); });
    connect(rough, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("rough", [&](DObject &o) { o.mat.roughness = v; }); });
    connect(emis, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("emis", [&](DObject &o) { o.mat.emission = v; }); });
    connect(flat, &QCheckBox::toggled, this, [=, this](bool v) { objEdit("flat", [&](DObject &o) { o.mat.shadeless = v; }); });
    connect(lKind, &QComboBox::activated, this, [=, this](int) { if (!*syncing) m->changeLightKind(lKind->currentText()); });
    connect(lEnergy, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("lE", [&](DObject &o) { o.light.energy = v; }); });
    connect(lSpot, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("lS", [&](DObject &o) { o.light.spotSize = v; }); });
    connect(lBlend, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("lB", [&](DObject &o) { o.light.spotBlend = v; }); });
    connect(lRad, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("lR", [&](DObject &o) { o.light.radius = v; }); });
    connect(cOrtho, &QCheckBox::toggled, this, [=, this](bool v) { objEdit("cO", [&](DObject &o) { o.cam.ortho = v; }); });
    connect(cFov, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("cF", [&](DObject &o) { o.cam.fov = v; }); });
    connect(cScale, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("cS", [&](DObject &o) { o.cam.orthoScale = v; }); });

    connect(physOn, &QCheckBox::toggled, this, [=, this](bool v) { objEdit("pOn", [&](DObject &o) { o.phys.enabled = v; }); });
    connect(physType, &QComboBox::activated, this, [=, this](int i) { objEdit("pT", [&](DObject &o) { o.phys.active = i == 0; }); });
    connect(mass, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("pM", [&](DObject &o) { o.phys.mass = v; }); });
    connect(fric, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("pF", [&](DObject &o) { o.phys.friction = v; }); });
    connect(bounce, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { objEdit("pB", [&](DObject &o) { o.phys.bounce = v; }); });
    connect(planarPhysics,&QCheckBox::toggled,this,[=,this](bool value){objEdit("pPlane",[&](DObject &o){o.style["physics.planar"]=value;});});
    connect(collisionMargin,&QDoubleSpinBox::valueChanged,this,[=,this](double value){objEdit("pMargin",[&](DObject &o){o.style["physics.margin"]=value;});});
    connect(surfaceThickness,&QDoubleSpinBox::valueChanged,this,[=,this](double value){objEdit("pThickness",[&](DObject &o){o.style["physics.thickness"]=value;});});

    auto worldEdit = [=, this](const QString &tag, std::function<void(World &)> f) {
        if (*syncing) return;
        touch(tag);
        f(m->design.world);
        m->notify();
    };
    connect(wTransparent, &QCheckBox::toggled, this, [=, this](bool v) { worldEdit("wT", [&](World &w) { w.transparent = v; }); });
    connect(wGrad, &QCheckBox::toggled, this, [=, this](bool v) { worldEdit("wG", [&](World &w) { w.gradient = v; }); });
    connect(wAmb, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { worldEdit("wA", [&](World &w) { w.ambient = v; }); });
    connect(wFog, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { worldEdit("wF", [&](World &w) { w.fog = v; }); });
    connect(wGrav, &QDoubleSpinBox::valueChanged, this, [=, this](double v) { worldEdit("wGr", [&](World &w) { w.gravity = v; }); });
    connect(wFloor, &QCheckBox::toggled, this, [=, this](bool v) { worldEdit("wFl", [&](World &w) { w.floor = v; }); });
    connect(oGrid, &QCheckBox::toggled, this, [canvas](bool v) { canvas->showGrid = v; canvas->update(); });
    connect(oAxes, &QCheckBox::toggled, this, [canvas](bool v) { canvas->showAxes = v; canvas->update(); });
    connect(oGiz, &QCheckBox::toggled, this, [canvas](bool v) { canvas->showGizmos = v; canvas->update(); });

    // ---- toolbar
    connect(editBtn, &QPushButton::clicked, this, [this, m, editBtn] {
        m->toggleEdit();
        editBtn->setChecked(m->editMode);
        if (!m->editMode && editBtn->isChecked()) editBtn->setChecked(false);
        if (m->editMode == false && m->active() == nullptr) say("Select a mesh object, then press Edit Mode", true);
    });
    auto syncMode = [=, this] {
        QSignalBlocker b(editBtn);
        editBtn->setChecked(m->editMode);
        for (QWidget *w : {static_cast<QWidget *>(vBtn), static_cast<QWidget *>(eBtn), static_cast<QWidget *>(fBtn), static_cast<QWidget *>(meshBtn)}) w->setVisible(m->editMode);
        vBtn->setChecked(m->selMode == SelMode::Vertex);
        eBtn->setChecked(m->selMode == SelMode::Edge);
        fBtn->setChecked(m->selMode == SelMode::Face);
        objBtn->setVisible(true);
    };
    connect(m, &BuildModel::modeChanged, this, syncMode);
    syncMode();
    connect(vBtn, &QPushButton::clicked, this, [m] { m->setSelMode(SelMode::Vertex); });
    connect(eBtn, &QPushButton::clicked, this, [m] { m->setSelMode(SelMode::Edge); });
    connect(fBtn, &QPushButton::clicked, this, [m] { m->setSelMode(SelMode::Face); });
    connect(wireBtn, &QPushButton::clicked, this, [canvas] { canvas->setShading(BuildCanvas::Wire); });
    connect(solidBtn, &QPushButton::clicked, this, [canvas] { canvas->setShading(BuildCanvas::Solid); });
    connect(rendBtn, &QPushButton::clicked, this, [canvas] { canvas->setShading(BuildCanvas::Rendered); });
    connect(materialBtn, &QPushButton::clicked, this, [canvas] { canvas->setShading(BuildCanvas::MaterialPreview); });
    connect(fixedBtn, &QPushButton::clicked, this, [canvas] { canvas->setCameraView(true); });
    connect(fitBtn, &QPushButton::clicked, this, [canvas] { canvas->frameAll(); });
    connect(undoBtn, &QPushButton::clicked, this, [m] { m->undo(); });
    connect(redoBtn, &QPushButton::clicked, this, [m] { m->redo(); });
    connect(canvas, &BuildCanvas::message, this, [this](const QString &t) { say(t); });

    // ---- animation
    auto *ticker = new QTimer(this);
    ticker->setInterval(16);
    auto clockT = std::make_shared<QElapsedTimer>();
    connect(ticker, &QTimer::timeout, this, [=, this] {
        if(!m->playing){ticker->stop();return;}
        const qint64 dt = clockT->restart();
        if(!m->previewCueId.isEmpty()){for(auto v:m->design.animationCues){const auto cue=v.toObject();if(cue["id"].toString()!=m->previewCueId)continue;m->previewCueElapsed+=dt;const auto sample=compileAnimationCue(m->design,cue).sample(m->previewCueElapsed,m->previewCueStep);if(sample.visible)m->setTime(sample.frame);else{m->setTime(cue["end"].toDouble());m->playing=false;ticker->stop();}return;}}
        qint64 t = m->t + dt;
        if (t >= m->design.durationMs) {
            if (m->design.loop) {
                t = m->design.startMs;
            } else {
                t = m->design.durationMs;
                m->playing = false;
                ticker->stop();
            }
        }
        m->setTime(t);
    });
    auto togglePlay = [=, this] {
        m->playing = !m->playing;
        if (m->playing) {
            if(!m->previewCueId.isEmpty()){m->previewCueElapsed=0;m->previewCueStep=0;for(auto v:m->design.animationCues)if(v.toObject()["id"].toString()==m->previewCueId)m->t=v.toObject()["start"].toDouble();}
            if (m->previewCueId.isEmpty()&&(m->t >= m->design.durationMs || m->t < m->design.startMs)) m->t = m->design.startMs;
            clockT->start();
            ticker->start();
        } else {
            ticker->stop();
        }
        refreshTime();
        canvas->update();
        m->notify();
    };
    connect(play, &QPushButton::clicked, this, togglePlay);
    connect(m,&BuildModel::cuePlaybackRequested,this,togglePlay);
    connect(toStart, &QPushButton::clicked, this, [m] { m->setTime(m->design.startMs); });
    connect(loop, &QCheckBox::toggled, this, [m](bool on) { m->design.loop = on; });
    connect(dur, &QDoubleSpinBox::valueChanged, this, [=, this](double s) {
        if (*syncing) return;
        m->design.durationMs = qRound(s * 1000);
        if (m->t > m->design.durationMs) m->t = m->design.durationMs;
        m->notify();
        m->setTime(m->t);
    });
    connect(m, &BuildModel::changed, this, [=, this] {
        *syncing = true;
        if (!dur->hasFocus()) dur->setValue(m->design.durationMs / 1000.0);
        loop->setChecked(m->design.loop);
        autoKey->setChecked(m->autoKey);
        *syncing = false;
    });
    connect(interp, &QComboBox::activated, this, [=, this](int) { m->setInterpolation(interp->currentText()); });
    connect(autoKey, &QCheckBox::toggled, this, [m](bool on) { m->autoKey = on; });
    connect(keyBtn, &QPushButton::clicked, this, [m] { m->insertKey(); });
    connect(delKey, &QPushButton::clicked, this, [m] { m->deleteKey(); });

    // Space plays and pauses; arrows move between keyframes; Ctrl + arrows step one frame
    auto shortcut = [this](const QKeySequence &k, std::function<void()> f) {
        auto *s = new QShortcut(k, this);
        s->setContext(Qt::WidgetWithChildrenShortcut);
        connect(s, &QShortcut::activated, this, std::move(f));
    };
    shortcut(Qt::Key_Space, togglePlay);
    shortcut(Qt::Key_Left, [m] { m->jumpKey(-1); });
    shortcut(Qt::Key_Right, [m] { m->jumpKey(+1); });
    shortcut(Qt::CTRL | Qt::Key_Left, [m] { m->stepFrame(-1); });
    shortcut(Qt::CTRL | Qt::Key_Right, [m] { m->stepFrame(+1); });
    shortcut(Qt::SHIFT | Qt::Key_Left, [m] { m->setTime(m->design.startMs); });
    shortcut(Qt::SHIFT | Qt::Key_Right, [m] { m->setTime(m->design.durationMs); });
    shortcut(Qt::CTRL | Qt::Key_S, [this] { save(); });

    shortcut(Qt::CTRL | Qt::Key_P,[m, this]{say(m->parentSelected()?"Parented to active layer, keeping transforms":"Select children, then the parent last. Parenting must not create a cycle.");});
    shortcut(Qt::ALT | Qt::Key_P,[m, this]{if(m->parentSelected(true))say("Parent cleared; transforms preserved");});
    auto iconButton=[](QAbstractButton *button,const QString &name){if(button->toolTip().isEmpty())button->setToolTip(button->text());button->setAccessibleName(button->toolTip());button->setIcon(uiIcon(name));button->setText({});button->setIconSize(QSize(15,15));button->setFixedSize(25,24);button->setStyleSheet("QAbstractButton{padding:3px;border:0;border-radius:4px;} QAbstractButton:hover{background:#26364b;} QAbstractButton:checked{background:#315b86;border-left:2px solid #83b9f1;}");};
    for(auto *button:QList<QAbstractButton*>{back,undoBtn,redoBtn,saveBtn,addBtn,editBtn,objBtn,meshBtn,vBtn,eBtn,fBtn,wireBtn,solidBtn,rendBtn,fixedBtn,fitBtn,actual,helpers})iconButton(button,button==undoBtn?"undo":button==redoBtn?"redo":button->text());
    for(int i=0;i<tabBtns.size();++i)iconButton(tabBtns[i],tabNames[i]);
    for(auto *button:QList<QAbstractButton*>{play,toStart,keyBtn,delKey,graphBtn})iconButton(button,button==play?"play":button==keyBtn?"key":button==delKey?"delete":button==graphBtn?"graph":"back");
    interp->hide();delKey->hide();
    for(auto *button:QList<QCheckBox*>{loop,autoKey}){button->setToolTip(button==loop?"Loop playback":"Auto key changed properties");button->setIcon(uiIcon(button==loop?"loop":"key"));button->setText({});button->setFixedSize(25,22);button->setStyleSheet("QCheckBox{border-radius:4px;padding:2px;} QCheckBox:checked{background:#496d9a;} QCheckBox::indicator{width:0;height:0;}");}
    auto *viewButton=new QToolButton;viewButton->setObjectName("buildViewMode");viewButton->setIcon(uiIcon("rendered"));viewButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);viewButton->setToolTip("Viewport appearance only; export always uses Final. Z cycles modes.");viewButton->setPopupMode(QToolButton::InstantPopup);
    auto *viewMenu=new QMenu(viewButton);
    const QList<QPushButton*> modes{wireBtn,solidBtn,rendBtn,materialBtn};
    const QStringList modeNames{"Wireframe","Work","Final","Material"};
    int modeWidth=0;for(const auto &name:modeNames)modeWidth=qMax(modeWidth,viewButton->fontMetrics().horizontalAdvance(name));viewButton->setFixedWidth(modeWidth+48);
    const QStringList modeHints{"Inspect mesh edges","Plain surfaces and readable typography without finishing effects","Output renderer with authored lighting and effects","Surface styling with neutral mesh lighting"};
    for(int mode:{1,3,2,0}){auto *button=modes[mode];auto *action=viewMenu->addAction(modeNames[mode]);action->setObjectName("buildViewMode_"+modeNames[mode]);action->setToolTip(modeHints[mode]);action->setCheckable(true);connect(action,&QAction::triggered,button,&QPushButton::click);connect(canvas,&BuildCanvas::shadingChanged,action,[action,mode](int active){action->setChecked(active==mode);});button->hide();}
    connect(canvas,&BuildCanvas::shadingChanged,viewButton,[=](int mode){viewButton->setText(modeNames[mode]);QSettings().setValue("build/viewMode",mode);for(int i=0;i<modes.size();++i)modes[i]->setChecked(i==mode);});
    viewButton->setMenu(viewMenu);tb->insertWidget(0,viewButton);
    canvas->setShading(qBound(0,QSettings().value("build/viewMode",int(BuildCanvas::Solid)).toInt(),3));
    viewButton->setText(modeNames[canvas->shading()]);for(auto *action:viewMenu->actions())action->setChecked(action->text()==viewButton->text());
    auto *qualityButton=new QToolButton;qualityButton->setObjectName("buildPreviewQuality");qualityButton->setPopupMode(QToolButton::InstantPopup);qualityButton->setToolTip("Preview resolution only. Auto uses half resolution during playback and transforms; full resolution when idle. Buffers are limited to 1080p pixel count.");
    auto *qualityMenu=new QMenu(qualityButton);qualityButton->setMenu(qualityMenu);
    QString quality=QSettings().value("build/previewQuality","Auto").toString();const QStringList qualities{"Auto","Full","Half","Quarter"};if(!qualities.contains(quality))quality="Auto";
    canvas->setProperty("previewQuality",quality);qualityButton->setText(quality);
    for(const auto &name:qualities){auto *action=qualityMenu->addAction(name);action->setCheckable(true);action->setChecked(name==quality);connect(action,&QAction::triggered,qualityButton,[=]{canvas->setProperty("previewQuality",name);qualityButton->setText(name);QSettings().setValue("build/previewQuality",name);for(auto *item:qualityMenu->actions())item->setChecked(item==action);canvas->update();});}
    tb->insertWidget(1,qualityButton);
    // buttons never keep the keyboard focus, so Space always means play / pause
    for (QAbstractButton *b : findChildren<QAbstractButton *>()) b->setFocusPolicy(Qt::NoFocus);

    connect(back, &QPushButton::clicked, this, &BuildWorkspace::closeRequested);
    connect(saveBtn, &QPushButton::clicked, this, [this] { save(); });
    connect(this, &BuildWorkspace::closeRequested, this, [=, this] {
        if (m->playing) togglePlay();
    });

    newElement();
    auto *audio=new DesignAudio(this);audio->error=[this](QString text){say(text,true);};
    auto syncAudio=[m,audio]{audio->sync(designAudioCues(m->design,m->t,m->playing));};
    connect(m,&BuildModel::timeChanged,audio,syncAudio);connect(m,&BuildModel::changed,audio,syncAudio);
}

void BuildWorkspace::say(const QString &text, bool error) {
    m_msg->setStyleSheet(error ? "color:#f87171;" : "color:palette(placeholder-text);");
    m_msg->setText("   " + text);
    QTimer::singleShot(4500, this, [this, text] { if (m_msg->text().trimmed() == text) m_msg->clear(); });
}

void BuildWorkspace::undoEdit() { m_model->undo(); }
void BuildWorkspace::redoEdit() { m_model->redo(); }
bool BuildWorkspace::hasUnsavedChanges() const {return m_savedJson!=m_model->design.toJson()||m_savedName!=m_name->text();}
bool BuildWorkspace::saveForClose(){save();return !m_editingId.isEmpty()&&!hasUnsavedChanges();}
bool BuildWorkspace::importFile(const QString &path){
    const auto suffix=QFileInfo(path).suffix().toLower();
    if(QStringList{"wav","mp3","flac","ogg","aac","m4a","opus","aiff"}.contains(suffix)){
        auto *probe=new QMediaPlayer(this);auto imported=std::make_shared<bool>(false);
        connect(probe,&QMediaPlayer::durationChanged,this,[=,this](qint64 duration){if(*imported||duration<=0)return;*imported=true;
            DObject object;object.id=Library::newId();object.type="Audio";object.name=m_model->uniqueName(QFileInfo(path).completeBaseName());
            object.style["audio.file"]=path;object.style["audio.duration"]=double(duration);object.style["audio.start"]=double(m_model->t);object.style["audio.volume"]=1.;object.captureDefaults();
            m_model->pushUndo();m_model->design.objects.append(object);m_model->design.durationMs=qMax(m_model->design.durationMs,int(qMin<qint64>(2147483647,m_model->t+duration)));m_model->select(object.id);m_model->notify();probe->deleteLater();
        });connect(probe,&QMediaPlayer::errorOccurred,this,[=,this](QMediaPlayer::Error,const QString &error){say(error,true);probe->deleteLater();});probe->setSource(QUrl::fromLocalFile(path));return true;
    }
    if(QStringList{"fbx","obj","gltf","glb","dae","stl","ply"}.contains(suffix)){
        QString error;auto objects=AssetImport::meshes(path,&error);
        if(objects.isEmpty()){QMessageBox::warning(this,"Import mesh",error);return false;}
        m_model->pushUndo();QSet<QString> ids;for(auto &object:objects){object.name=m_model->uniqueName(object.name);object.base.loc=m_model->design.cursor;object.captureDefaults();ids.insert(object.id);m_model->design.objects.append(object);}
        m_model->setSelection(ids,objects.last().id);m_model->notify();m_canvas->frameSelected();return true;
    }
    if(suffix=="cuebuild"||suffix=="json"){
        QFile file(path);if(!file.open(QIODevice::ReadOnly)){say(file.errorString(),true);return false;}
        const QString data=QString::fromUtf8(file.readAll());auto design=ElementDesign::fromJson(data);
        if(design.isEmpty()){say("The file does not contain a Build composition.",true);return false;}
        resolveDesignAssets(design,QFileInfo(path).absolutePath());
        const auto missing=missingDesignAsset(design);if(!missing.isEmpty()){say(missing,true);return false;}
        m_model->pushUndo();m_model->design=design;m_model->deselectAll();m_model->notify();m_canvas->frameAll();return true;
    }
    if(!m_model->addImage(path).isEmpty())return true;
    say("This file could not be imported.",true);return false;
}
void BuildWorkspace::exportFile(){
    QString selectedFilter;
    QString path=QFileDialog::getSaveFileName(this,"Export Build composition",m_name->text(),"Cuebase composition (*.cuebuild);;Portable composition with media (*.cuebuild);;PNG image (*.png);;PNG animation folder (*.frames)",&selectedFilter);if(path.isEmpty())return;
    if(selectedFilter.startsWith("Portable composition")){
        if(!path.endsWith(".cuebuild",Qt::CaseInsensitive))path+=".cuebuild";
        QProgressDialog progress("Collecting composition media…","Cancel",0,designAssets(m_model->design).size(),this);progress.setWindowModality(Qt::WindowModal);progress.setMinimumDuration(0);progress.setAutoClose(false);progress.setAutoReset(false);
        QString error;const bool saved=exportPortableDesign(m_model->design,path,&error,[&](int done,int total){progress.setMaximum(total);progress.setValue(done);QApplication::processEvents();return !progress.wasCanceled();});
        if(saved)say("Portable composition saved. Move the .cuebuild file and its sibling .assets folder together.");else say(error,true);return;
    }
    if(selectedFilter.startsWith("PNG animation")&&!path.endsWith(".frames",Qt::CaseInsensitive))path+=".frames";
    if(QFileInfo(path).suffix().compare("frames",Qt::CaseInsensitive)==0){
        FrameExportOptions options;options.size={m_lib->display.resX(),m_lib->display.resY()};options.setProjectFrameRate(m_model->fps,m_model->fpsBase);
        options.renderSettings.motionBlur=m_lib->display.motionBlur;options.renderSettings.shutter=m_lib->display.shutter;options.renderSettings.samples=m_lib->display.mbSamples;
        options.colorView=m_lib->display.colorView;options.colorLook=m_lib->display.colorLook;options.exposure=m_lib->display.exposure;options.gamma=m_lib->display.gamma;
        options.firstFrame=qMax(0,qFloor(double(m_model->design.startMs)*options.fps/(1000.*options.fpsBase)));
        options.endFrame=qCeil(double(m_model->design.durationMs)*options.fps/(1000.*options.fpsBase));
        QProgressDialog progress("Exporting PNG frames with transparency (no audio)…","Cancel",0,options.endFrame-options.firstFrame,this);
        progress.setWindowModality(Qt::WindowModal);progress.setMinimumDuration(0);
        const auto result=exportSceneFrames(m_model->design,path,options,[&](int done,int){progress.setValue(done);QApplication::processEvents();return !progress.wasCanceled();});
        if(result.success)say(QString("Saved %1 PNG frames to %2").arg(result.written).arg(path));
        else if(result.cancelled)say(QString("Export cancelled; %1 completed frames remain in %2").arg(result.written).arg(path));
        else say(result.error,true);return;
    }
    if(QFileInfo(path).suffix().compare("png",Qt::CaseInsensitive)==0){QImage image(m_lib->display.resX(),m_lib->display.resY(),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);RenderSettings settings;settings.motionBlur=m_lib->display.motionBlur;settings.shutter=m_lib->display.shutter;settings.samples=m_lib->display.mbSamples;settings.frameMs=1000.*m_model->fpsBase/qMax(1,m_model->fps);m_model->design.render(painter,QRectF(image.rect()),m_model->t,true,&settings);painter.end();const auto &display=m_lib->display;if(colorManagementActive(display.colorView,display.colorLook,display.exposure,display.gamma))applyColorManagement(image,display.colorView,display.colorLook,display.exposure,display.gamma);if(!image.save(path))say("Could not save the image.",true);return;}
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)){say(file.errorString(),true);return;}file.write(m_model->design.toJson().toUtf8());if(!file.commit())say(file.errorString(),true);
}

void BuildWorkspace::newElement() {
    m_cueEntry.clear();m_cueIteration=-1;m_cuePath.clear();m_cuePlacement.clear();
    m_editingId.clear();
    refreshElements();m_name->setText("New element");
    m_msg->clear();
    m_model->reset(ElementDesign::defaultScene());
    if(auto *camera=m_model->design.camera()) {
        camera->cam.fov=qRadiansToDegrees(2.0*qAtan((m_lib->display.resX()/kPxPerUnit)/20.0));
        camera->cam.orthoScale=m_lib->display.resX()/kPxPerUnit;
    }
    DObject rectangle;rectangle.id=Library::newId();rectangle.name="Rectangle";rectangle.type="Shape";
    rectangle.w=4;rectangle.h=2;rectangle.base.rot={90,0,0};rectangle.mat.color=Qt::white;rectangle.mat.shadeless=true;
    rectangle.style["shape"]="Rectangle";rectangle.captureDefaults();m_model->design.objects.append(rectangle);m_model->select(rectangle.id);m_model->notify();
    m_savedJson=m_model->design.toJson();m_savedName=m_name->text();m_draftName=m_name->text();
    m_canvas->frameAll();
}

void BuildWorkspace::editElement(const QString &id) {
    m_cueEntry.clear();m_cueIteration=-1;m_cuePath.clear();m_cuePlacement.clear();
    const Element *e = m_lib->element(id);
    if (!e) {
        newElement();
        return;
    }
    m_editingId = id;
    refreshElements();m_name->setText(e->name);
    m_msg->clear();
    m_model->reset(ElementDesign::fromJson(e->design));
    if(e->design.isEmpty()){
        auto design=ElementDesign::defaultScene();design.camera()->cam.orthoScale=m_lib->display.resX()/kPxPerUnit;design.durationMs=qMax(1,e->seconds)*1000;
        const int width=m_lib->display.resX(),height=m_lib->display.resY(),margin=width*3/100;
        auto position=[&](QSizeF size){double x=e->position.contains("Left")?margin:e->position.contains("Right")?width-margin-size.width():(width-size.width())/2;double y=e->position.contains("Bottom")?height-margin-size.height():margin;return QVector3D((x+size.width()/2-width/2.)/kPxPerUnit,0,(height/2.-y-size.height()/2)/kPxPerUnit);};
        auto make=[&](QString type,QSizeF size,QColor color){DObject object;object.id=Library::newId();object.type=type;object.name=e->name;object.w=size.width()/kPxPerUnit;object.h=size.height()/kPxPerUnit;object.base.rot={90,0,0};object.base.loc=position(size);object.mat.color=color;object.mat.shadeless=true;return object;};
        QImage image(e->image);
        if(e->type=="Logo"&&!image.isNull()){QSizeF size=image.size();size.scale(width,height*.14,Qt::KeepAspectRatio);auto object=make("Image",size,Qt::white);object.image=e->image;object.captureDefaults();design.objects.append(object);}
        else{const int px=e->type=="Logo"?height*7/100:e->type=="Banner"?height*55/1000:height*45/1000;QFont font("Segoe UI");font.setPixelSize(px);font.setBold(true);QFontMetrics metrics(font);const int pad=px*6/10;QSize size(metrics.horizontalAdvance(e->text)+pad*2,metrics.height()+pad);if(e->type=="Banner"&&e->position.contains("Center"))size.setWidth(width*6/10);
            auto background=make("Shape",size,e->type=="Logo"?QColor(0,0,0,120):QColor(e->color));background.name+=" background";background.style["shape"]="Rectangle";background.style["corner.radius"]=size.height()/(e->type=="Logo"?3.:4.);background.captureDefaults();design.objects.append(background);
            auto text=make("Text",size,e->type=="Logo"?QColor(e->color):QColor(Qt::white));text.text=e->text;text.fontPx=px;text.style["text.weight"]=700;text.captureDefaults();design.objects.append(text);
        }
        m_model->reset(design);m_model->select(design.objects.last().id);
    }
    if(const auto *effect=sceneEffect(m_model->design)){
        m_model->select(effect->id);
        if(auto *tab=findChild<QPushButton*>("buildTab_Effects"))tab->click();
    }
    m_savedJson=m_model->design.toJson();m_savedName=m_name->text();m_draftName=m_name->text();
    m_canvas->frameAll();
}

void BuildWorkspace::editCueSection(const QString &elementId,const QString &entryId,int iteration){editCuePath(elementId,{}, {entryId,QString::number(iteration)});}
void BuildWorkspace::editCuePath(const QString &elementId,const QString &placementId,const QStringList &path){
    if(path.size()<2||path.size()%2)return;Element root;if(!placementId.isEmpty()){auto *p=m_lib->placement(placementId);if(!p)return;root=m_lib->placementElement(*p);}else{const auto *e=m_lib->element(elementId);if(!e)return;root=*e;}
    const auto parent=cueBoardContext(root,path.mid(0,path.size()-2));QJsonObject entry;for(auto v:parent.cueBoard)if(v.toObject()["id"].toString()==path[path.size()-2])entry=v.toObject();if(entry.isEmpty())return;const auto source=ElementDesign::fromJson(parent.design);QJsonObject cue;for(auto c:source.animationCues)if(c.toObject()["id"]==entry["cue"])cue=c.toObject();if(cue.isEmpty())return;const auto section=cueBoardSection(root,path);auto design=section["design"].toString().isEmpty()?source:ElementDesign::fromJson(section["design"].toString());if(section["design"].toString().isEmpty()){design.animationCues={};design.startMs=cue["start"].toInt();design.durationMs=cue["end"].toInt();design.loop=false;}const auto fields=section["fields"].toObject();for(auto key:fields.keys())setStudioField(design,key,fields[key].toString());
    m_editingId=elementId;m_cuePath=path;m_cuePlacement=placementId;m_cueEntry=entry["id"].toString();m_cueIteration=path.last().toInt();m_cueBase=QString::fromUtf8(QJsonDocument(bakeCueBoard(root)).toJson(QJsonDocument::Compact));refreshElements();m_name->setText(section["name"].toString(parent.name+" · "+cue["name"].toString()+QString(" · Play %1").arg(m_cueIteration+1)));m_model->reset(design);m_model->setTime(design.startMs);m_savedJson=m_model->design.toJson();m_savedName=m_draftName=m_name->text();m_canvas->frameAll();say("Independent cue keyframes. New cues become children of this section.");
}

void BuildWorkspace::newEffect() {
    m_cueEntry.clear();m_cueIteration=-1;m_cuePath.clear();m_cuePlacement.clear();
    m_editingId.clear();refreshElements();m_name->setText("New effect");m_msg->clear();
    m_model->reset(sceneEffectPreset("Fade"));m_model->select(sceneEffect(m_model->design)->id);
    m_savedJson=m_model->design.toJson();m_savedName=m_name->text();m_draftName=m_name->text();
    m_canvas->frameAll();
    if(auto *tab=findChild<QPushButton*>("buildTab_Effects"))tab->click();
}

void BuildWorkspace::save() {
    const QString requestedName=m_name->text();
    QString n = requestedName.trimmed();QString semanticRole,semanticPlaylist;
    if(m_cueIteration<0){static const QRegularExpression tag("^\\{\\{element:([a-z-]+)\\|([^{}\\r\\n]+)\\}\\}$");const auto match=tag.match(n);if(match.hasMatch()){semanticRole=match.captured(1);n=match.captured(2).trimmed();const QMap<QString,QString> categories{{"logo","Logos"},{"main-logo","Logos"},{"overlay","Overlays"},{"lower-third","Overlays"},{"title","Titles"},{"ticker","Overlays"},{"clock","Overlays"},{"break","Break screens"},{"ident","Break screens"},{"replay","Effects"},{"effect","Effects"},{"credits","Titles"}};if(!categories.contains(semanticRole)){say("Unknown scene category. Use logo, main-logo, overlay, lower-third, title, ticker, clock, break, ident, replay, effect or credits.",true);return;}semanticPlaylist=categories.value(semanticRole);m_name->setText(n);if(auto *camera=m_model->design.camera())camera->style["studio.sceneRole"]=semanticRole;}}

    if (n.isEmpty()) {
        say("Give the element a name first.", true);
        m_name->setFocus();
        return;
    }
    if (m_model->design.isEmpty()) {
        say("Add at least one mesh, text or image.", true);
        return;
    }
    if (m_model->editMode) m_model->exitEdit();
    if(m_cueIteration>=0){
        Element root;Placement *placement=nullptr;Element *source=nullptr;if(!m_cuePlacement.isEmpty()){placement=m_lib->placement(m_cuePlacement);if(!placement){say("Timeline copy was removed.",true);return;}root=m_lib->placementElement(*placement);}else{for(auto &e:m_lib->elements)if(e.id==m_editingId)source=&e;if(!source){say("Source element was removed.",true);return;}root=*source;}
        if(QString::fromUtf8(QJsonDocument(bakeCueBoard(root)).toJson(QJsonDocument::Compact))!=m_cueBase){say("This board changed elsewhere. Reopen its cue before saving.",true);return;}const auto before=root;auto section=cueBoardSection(root,m_cuePath);section["design"]=m_model->design.toJson();section["name"]=n;auto fields=section["fields"].toObject();for(auto it=fields.begin();it!=fields.end();++it)if(auto *o=m_model->design.find(it.key()))it.value()=o->text;section["fields"]=fields;
        if(section.contains("board")){auto children=section["board"].toArray();for(auto value:m_model->design.animationCues){bool found=false;for(auto existing:children)found|=existing.toObject()["cue"]==value.toObject()["id"];if(!found)children.append(newCueBoardEntry(value.toObject()));}section["board"]=children;}
        if(!setCueBoardSection(root,m_cuePath,section)){say("Cue section no longer exists.",true);return;}QString error;if(placement&&cueEditGuard&&!cueEditGuard(before,root,*placement,&error)){say(error,true);return;}const auto oldPlacement=placement?*placement:Placement{};const auto oldSnapshot=placement?placement->boardSnapshot:QJsonObject{};if(placement){const auto plan=compileCueBoard(ElementDesign::fromJson(root.design),root.cueBoard,root.cueBoardSettings);if(!plan.error.isEmpty()){say(plan.error,true);return;}placement->boardSnapshot=bakeCueBoard(root);if(plan.duration()>0){if(placement->lengthMs()==placement->boardCycleMs)placement->durationMs=plan.duration();placement->boardCycleMs=plan.duration();}}else source->cueBoard=root.cueBoard;if(!m_lib->saveChecked(&error)){if(placement)*placement=oldPlacement;else source->cueBoard=before.cueBoard;say(error,true);return;}m_cueBase=QString::fromUtf8(QJsonDocument(bakeCueBoard(root)).toJson(QJsonDocument::Compact));m_savedJson=m_model->design.toJson();m_savedName=m_draftName=m_name->text();if(placement)emit m_lib->placementsChanged();else emit m_lib->elementsChanged();say("Saved independent cue keyframes");return;
    }
    if(const auto *existing=m_lib->element(m_editingId)){const auto saved=ElementDesign::fromJson(existing->design).animationCues;for(int i=0;i<m_model->design.animationCues.size();++i){auto cue=m_model->design.animationCues[i].toObject();for(auto value:saved){const auto prior=value.toObject();if(prior["id"]==cue["id"])for(const auto *key:{"trigger","triggerMs"})if(prior.contains(key))cue[key]=prior[key];}m_model->design.animationCues[i]=cue;}}
    if(const auto *existing=m_lib->element(m_editingId)){
        const auto saved=ElementDesign::fromJson(m_savedJson),latest=ElementDesign::fromJson(existing->design);
        for(auto &object:m_model->design.objects)if(object.style["studio.editable"].toBool())if(const auto *prior=saved.find(object.id))if(const auto *current=latest.find(object.id)){
            if(object.text==prior->text)object.text=current->text;if(object.image==prior->image)object.image=current->image;
        }
    }
    const QString json = m_model->design.toJson();
    const int secs = qMax(1, int(qCeil(m_model->design.durationMs / 1000.0)));
    QString color = "#6366f1";
    for (const DObject &o : m_model->design.objects)
        if (o.isRenderable()) { color = o.mat.color.name(); break; }
    const auto previousElements=m_lib->elements;
    const auto previousPlaylists=m_lib->elementPlaylists;
    const auto previousLogo=m_lib->display.channelLogoElement;
    const auto previousId=m_editingId;
    bool updated = false;
    for (Element &e : m_lib->elements)
        if (e.id == m_editingId) {
            e.name = n;
            e.design = json;
            e.type = sceneEffect(m_model->design)?"Effect":"Custom";
            e.seconds = secs;if(!semanticPlaylist.isEmpty())e.playlist=semanticPlaylist;refreshElementThumbnail(e);
            updated = true;
        }
    if (!updated) {
        Element e;
        e.id = Library::newId();
        e.name = n;
        e.type = sceneEffect(m_model->design)?"Effect":"Custom";
        if(e.type=="Effect")e.playlist="Effects";if(!semanticPlaylist.isEmpty())e.playlist=semanticPlaylist;
        e.design = json;
        e.seconds = secs;
        e.color = color;
        e.previewMs=m_model->t;if(e.previewMs==0&&!m_model->design.animationCues.isEmpty())e.previewMs=m_model->design.animationCues.first().toObject()["end"].toDouble();refreshElementThumbnail(e);
        if(!editElementPresentation(e,this)){m_name->setText(requestedName);return;}
        if(!m_lib->elementPlaylists.contains(e.playlist))m_lib->elementPlaylists.append(e.playlist);
        m_lib->elements.append(e);m_name->setText(e.name);
        m_editingId = e.id;
    }
    if(semanticRole=="main-logo")m_lib->display.channelLogoElement=m_editingId;
    if(!semanticPlaylist.isEmpty()&&!m_lib->elementPlaylists.contains(semanticPlaylist))m_lib->elementPlaylists.append(semanticPlaylist);
    QString error;
    if(!m_lib->saveChecked(&error)){
        m_lib->elements=previousElements;
        m_lib->elementPlaylists=previousPlaylists;
        m_lib->display.channelLogoElement=previousLogo;
        m_editingId=previousId;
        m_name->setText(requestedName);
        say("Could not save screen element: "+error,true);
        return;
    }
    emit m_lib->elementsChanged();
    m_savedJson=m_model->design.toJson();m_savedName=m_name->text();m_draftName=m_name->text();
    say("Saved to screen elements");
}

void BuildWorkspace::refreshElements(){QSignalBlocker block(m_elementPicker);const auto name=m_name->text();m_elementPicker->clear();for(const auto &e:m_lib->elements)m_elementPicker->addItem(e.name,e.id);m_elementPicker->setCurrentIndex(m_elementPicker->findData(m_editingId));m_name->setText(name);}
void BuildWorkspace::capturePreview(){if(m_cueIteration>=0){say("Edit the library thumbnail from the source scene or Studio.");return;}const bool fresh=m_editingId.isEmpty();if(fresh||hasUnsavedChanges()){save();if(fresh||m_editingId.isEmpty()||hasUnsavedChanges())return;}for(auto &e:m_lib->elements)if(e.id==m_editingId){Element draft=e;draft.previewMs=m_model->t;if(editElementPresentation(draft,this)){e=draft;m_name->setText(e.name);m_savedName=e.name;if(!m_lib->elementPlaylists.contains(e.playlist))m_lib->elementPlaylists.append(e.playlist);m_lib->commitElements();}break;}}
