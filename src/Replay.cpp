#include "Replay.h"
#include "SceneEffects.h"
#include "UiControls.h"
#include "DesignAudio.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoFrame>
#include <QFileInfo>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>

Replay::Replay(Playout *p,Library *l):QObject(p),play(p),library(l){
    player=new QMediaPlayer(this);audio=new QAudioOutput(this);sink=new QVideoSink(this);
    player->setAudioOutput(audio);player->setVideoSink(sink);
    layers=new DesignAudio(this);layers->created=[this](QMediaPlayer *source,QAudioOutput *output){emit play->audioSourceCreated(source,output);};layers->error=[this](QString reason){emit failed("Replay layer audio: "+reason);};
    connect(sink,&QVideoSink::videoFrameChanged,this,[this](const QVideoFrame &v){if(v.isValid()){frame=v.toImage();emit changed();}});
    connect(player,&QMediaPlayer::mediaStatusChanged,this,[this](QMediaPlayer::MediaStatus s){if(pending>=0&&(s==QMediaPlayer::LoadedMedia||s==QMediaPlayer::BufferedMedia)){const auto seek=pending;pending=-1;if(seek>0)player->setPosition(seek);if(active()&&phase!=Leave)player->play();}});
    connect(player,&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString &reason){if(active()){cancel();emit failed("Replay stopped: "+reason);}});
    connect(play,&Playout::stateChanged,this,[this]{if(active()&&!play->isLive())cancel();});
    timer.setInterval(20);connect(&timer,&QTimer::timeout,this,&Replay::tick);clock.start();
    QTimer::singleShot(0,this,[this]{emit play->audioSourceCreated(player,audio);});
}
bool Replay::start(qint64 from,qint64 to,const QString &entry,const QString &exit,bool pause,QString *error){
    auto fail=[&](QString message){if(error)*error=message;return false;};
    if(!play->isLive())return fail("Start or pause the program before taking a replay to air.");
    if(active())return fail("Return from the current replay first.");
    if(from<0||to<=from||to>play->totalMs())return fail("Choose a valid range within the program timeline.");
    if(to>play->playheadMs())return fail("Replay ends after the current program position. Select frames that have already played.");
    for(int i=0;i<play->segments().size();++i){const auto &s=play->segments()[i];const auto start=play->segmentStartMs(i);if(start>=to||start+s.ms<=from)continue;
        if(!s.file.isEmpty()&&!QFileInfo(s.file).isFile())return fail("Replay media is unavailable: "+s.title);
        if(s.kind=="Live"&&s.file.isEmpty())return fail("This live segment has no recording. Replay requires recorded program media.");
    }
    auto effect=[&](const QString &id){if(id.startsWith("preset:"))return sceneEffectPreset(id.mid(7));if(const auto *e=library->element(id)){auto d=ElementDesign::fromJson(e->design);if(sceneEffect(d))return d;}return sceneEffectPreset("Fade");};
    entryEffect=effect(entry);exitEffect=effect(exit);segments=play->segments();starts.clear();for(int i=0;i<segments.size();++i)starts.append(play->segmentStartMs(i));
    position=from;end=to;elapsed=phaseTime=0;index=-1;resume=pause&&play->state()==Playout::Playing;
    if(resume)play->togglePause();phase=Enter;play->setReplayMix(0);last=lastReady=clock.elapsed();sync();timer.start();emit changed();return true;
}
void Replay::sync(){
    int next=-1;for(int i=0;i<segments.size();++i)if(position>=starts[i]&&position<starts[i]+segments[i].ms){next=i;break;}
    if(next<0){returnToProgram();return;}segment=segments[next];
    if(index!=next){index=next;frame={};pending=-1;player->stop();player->setSource({});
        if(!segment.file.isEmpty()){pending=segment.offsetMs+position-starts[index];player->setSource(QUrl::fromLocalFile(segment.file));}
    }
    const auto mix=phase==Enter?sceneEffectValue(entryEffect,"scene.mix",phaseTime,0):phase==Leave?1-sceneEffectValue(exitEffect,"scene.mix",phaseTime,0):1;
    play->setReplayMix(mix);audio->setVolume(float(play->volume()*library->layers.value(0).volume/100.*qBound(0.,mix,1.)));audio->setMuted(play->muted()||library->layers.value(0).muted);
    QList<DesignAudioCue> cues;QSet<QString> drawn;const double gain=play->muted()?0:play->volume()*qBound(0.,mix,1.),speed=qBound(.1,sceneEffectValue(entryEffect,"scene.speed",elapsed,1),2.);
    auto append=[&](const Element &element,qint64 time,double level,bool controlled,bool advancing){if(element.type!="Custom"||drawn.contains(element.id))return;drawn.insert(element.id);auto it=audioDesigns.find(element.id);if(it==audioDesigns.end()||it->first!=element.design)it=audioDesigns.insert(element.id,{element.design,ElementDesign::fromJson(element.design)});auto added=designAudioCues(it->second,time,phase!=Leave&&advancing,"replay/"+element.id+"/",level,!controlled);for(auto &cue:added)cue.rate*=speed;cues.append(added);};
    for(const auto &element:library->elements){const auto playback=play->graphicPlayback(element,position);if(playback.visible)append(playback.resolved(element),playback.time,gain,playback.controlled,playback.advancing);}
    for(const auto &placement:library->placements){if(position<placement.startMs||position>=placement.startMs+placement.lengthMs())continue;const auto layer=library->layers.value(qBound(0,placement.layer,3));const auto level=layer.muted?0:gain*layer.volume/100.;if(placement.type=="Element"){if(layer.visible)if(const auto *element=library->element(placement.refId)){const auto playback=play->graphicPlayback(*element,position,placement.startMs,placement.offsetMs);if(playback.visible)append(playback.resolved(*element),playback.time,level,playback.controlled,playback.advancing);}}else if(!placement.file.isEmpty())cues.append({"replay/placement/"+placement.id,placement.file,position-placement.startMs+placement.offsetMs,level,phase!=Leave,speed});}
    layers->sync(cues);
}
void Replay::tick(){
    const auto now=clock.elapsed();const auto dt=qBound(qint64(0),now-last,qint64(250));last=now;
    if(!active())return;
    // A decoder must be ready before advancing its playback clock.
    if((pending>=0||(player->hasVideo()&&frame.isNull()))&&phase!=Leave){if(now-lastReady>15000){cancel();emit failed("Replay media did not become ready in time.");}return;}
    lastReady=now;phaseTime+=dt;elapsed+=dt;
    if(phase==Leave){if(phaseTime>=exitEffect.durationMs){cancel();return;}}
    else {
        const double speed=qBound(.1,sceneEffectValue(entryEffect,"scene.speed",elapsed,1),2.);
        player->setPlaybackRate(speed);position=qMin(end,position+qRound64(dt*speed));
        if(position>=end){position=qMax<qint64>(0,end-1);returnToProgram();}
        else if(phase==Enter&&phaseTime>=entryEffect.durationMs){phase=Running;phaseTime=0;}
    }
    sync();emit changed();
}
void Replay::returnToProgram(){if(!active()||phase==Leave)return;phase=Leave;phaseTime=0;player->pause();emit changed();}
void Replay::cancel(){if(!active())return;phase=Off;timer.stop();player->stop();player->setSource({});layers->sync({});frame={};pending=-1;play->setReplayMix(0);const bool shouldResume=resume;resume=false;if(shouldResume&&play->state()==Playout::Paused)play->togglePause();play->finishReplay();emit changed();}
QImage Replay::composite(const QImage &program,const QImage &picture)const{
    if(phase==Enter)return blendSceneEffect(program,picture,entryEffect,phaseTime);
    const auto processed=applySceneEffect(picture,entryEffect,elapsed);
    if(phase==Leave)return blendSceneEffect(processed,program,exitEffect,phaseTime);
    return phase==Running?processed:program;
}
bool showReplayDialog(Playout *play,Library *library,qint64 from,qint64 to,QWidget *parent){
    QDialog dialog(parent);dialog.setWindowTitle("Replay selected frames");dialog.setWindowFlag(Qt::FramelessWindowHint);dialog.setAttribute(Qt::WA_TranslucentBackground);dialog.setMinimumWidth(430);
    auto *outer=new QVBoxLayout(&dialog);outer->setContentsMargins(0,0,0,0);auto *card=new QFrame;card->setObjectName("replayCard");outer->addWidget(card);auto *layout=new QVBoxLayout(card);layout->setContentsMargins(18,18,18,18);layout->addWidget(new QLabel("REPLAY SELECTED FRAMES"));auto *form=new QFormLayout;layout->addLayout(form);
    dialog.setStyleSheet("QWidget{font-size:11px;color:#dce1e8;}QFrame#replayCard{background:#2b2e34;border:1px solid #535b67;border-radius:12px;}QLabel,QCheckBox{background:transparent;}QPushButton,QComboBox,QDoubleSpinBox{background:#414650;border:0;border-radius:5px;padding:5px;}");
    const double frameMs=1000.*library->display.fpsBase/qMax(1,library->display.fps);
    auto *start=new ValueInput,*end=new ValueInput;for(auto *input:{start,end}){input->setDecimals(0);input->setRange(0,play->totalMs()/frameMs);input->setSingleStep(1);}start->setValue(qRound64(from/frameMs));end->setValue(qRound64(to/frameMs));form->addRow("Replay from frame",start);form->addRow("End frame (exclusive)",end);
    auto *entry=new QComboBox,*exit=new QComboBox;for(auto *box:{entry,exit}){for(const auto &name:sceneEffectPresets())box->addItem(name,"preset:"+name);for(const auto &e:library->elements)if(e.type=="Effect")box->addItem(e.name,e.id);}
    if(const auto *s=play->current())if(const auto *p=library->program(s->programId)){int i=entry->findData(p->replayInEffect),o=exit->findData(p->replayOutEffect);if(i>=0)entry->setCurrentIndex(i);if(o>=0)exit->setCurrentIndex(o);}
    form->addRow("Enter replay",entry);form->addRow("Return to program",exit);auto *pause=new QCheckBox("Pause current scene during replay");pause->setChecked(false);layout->addWidget(pause);
    auto *hint=new QLabel("Unchecked: the program keeps advancing. Checked: it resumes from the same point after replay.");hint->setWordWrap(true);layout->addWidget(hint);auto *error=new QLabel;error->setWordWrap(true);error->setStyleSheet("color:#edb27b");layout->addWidget(error);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText("Take replay to air");layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{QString reason;if(play->replay()->start(qRound64(start->value()*frameMs),qRound64(end->value()*frameMs),entry->currentData().toString(),exit->currentData().toString(),pause->isChecked(),&reason))dialog.accept();else error->setText(reason);});return dialog.exec()==QDialog::Accepted;
}
