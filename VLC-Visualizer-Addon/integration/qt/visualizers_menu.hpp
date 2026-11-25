// SPDX-License-Identifier: GPL-2.0-or-later
// Built into VLC's own Qt interface, alongside its existing main menu.
#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
namespace InlineVisualizers {
inline QString suitePath() {
    QString override=qEnvironmentVariable("VLC_VISUALIZERS_TEST_SUITE");
    if(override.isEmpty())override=qEnvironmentVariable("VLC_VISUALIZERS_SUITE");
    if(!override.isEmpty())return override;
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath()+QStringLiteral("/visualizers");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+QStringLiteral("/vlc-visualizers");
#endif
}
inline QString executable(const QDir& suite,const char* name){
    return suite.filePath(QString::fromLatin1(name)
#ifdef Q_OS_WIN
        +QStringLiteral(".exe")
#endif
    );
}
inline QJsonObject cache(const QDir& suite){QFile f(suite.filePath(QStringLiteral("installed-visualizers.json")));if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)return {};return QJsonDocument::fromJson(f.readAll()).object();}
inline void launch(QMenu* owner,const QDir& suite,const QJsonObject& entry,bool settings){
    const QString package=suite.filePath(entry.value(QStringLiteral("package")).toString());
    QProcess check;check.start(executable(suite,"VisualizersCtl"),QStringList()<<QStringLiteral("check")<<QStringLiteral("--suite")<<suite.path()<<QStringLiteral("--package")<<package);
    if(!check.waitForFinished(5000)){check.kill();check.waitForFinished();QMessageBox::warning(owner,QStringLiteral("Visualizer unavailable"),QStringLiteral("The package check could not finish."));return;}
    if(check.exitStatus()!=QProcess::NormalExit||check.exitCode()!=0){QMessageBox::information(owner,QStringLiteral("Visualizer unavailable"),QStringLiteral("The visualizer is missing or incompatible. Rescan after installing or removing it."));return;}
    QStringList args;args<<(settings?QStringLiteral("--settings"):QStringLiteral("--visualizer"))<<entry.value(QStringLiteral("id")).toString()<<QStringLiteral("--vlc-pid")<<QString::number(QCoreApplication::applicationPid());
    // Native self-tests close after 90 frames; software adapters can need more
    // than the original-engine fixture's five-second cutoff to reach that.
    if(qEnvironmentVariableIsSet("VLC_VISUALIZERS_SELF_TEST"))args<<QStringLiteral("--self-test")<<QStringLiteral("--background")<<QStringLiteral("--quit-after")<<QStringLiteral("20000")<<QStringLiteral("--profile")<<suite.filePath(QStringLiteral("menu-test-profile"));
    if(!QProcess::startDetached(executable(suite,"Visualizers"),args,suite.path()))QMessageBox::warning(owner,QStringLiteral("Visualizers"),QStringLiteral("The visualizer host could not start."));
}
inline void populate(QMenu* menu,const QDir& suite){
    menu->clear();int count=0;
    for(const QJsonValue& value:cache(suite).value(QStringLiteral("visualizers")).toArray()){
        if(++count>128)break;const QJsonObject entry=value.toObject();
        QMenu* item=menu->addMenu(entry.value(QStringLiteral("name")).toString());
        const QString package=suite.filePath(entry.value(QStringLiteral("package")).toString());
        const bool present=QFileInfo::exists(QDir(package).filePath(QStringLiteral("manifest.json")))&&QFileInfo::exists(QDir(package).filePath(entry.value(QStringLiteral("entrypoint")).toString()));
        QAction* open=item->addAction(QStringLiteral("Open"));open->setEnabled(present);QObject::connect(open,&QAction::triggered,menu,[=]{launch(menu,suite,entry,false);});
        QAction* settings=item->addAction(QStringLiteral("Settings…"));settings->setEnabled(present);QObject::connect(settings,&QAction::triggered,menu,[=]{launch(menu,suite,entry,true);});
        QAction* close=item->addAction(QStringLiteral("Close"));QObject::connect(close,&QAction::triggered,menu,[=]{QProcess::startDetached(executable(suite,"Visualizers"),QStringList()<<QStringLiteral("--close")<<entry.value(QStringLiteral("id")).toString()<<QStringLiteral("--vlc-pid")<<QString::number(QCoreApplication::applicationPid()),suite.path());});
    }
    if(!count)menu->addAction(QStringLiteral("No visualizers installed"))->setEnabled(false);
    QAction* closeAll=menu->addAction(QStringLiteral("Close all visualizers"));QObject::connect(closeAll,&QAction::triggered,menu,[=]{QProcess::startDetached(executable(suite,"Visualizers"),QStringList()<<QStringLiteral("--close")<<QStringLiteral("*")<<QStringLiteral("--vlc-pid")<<QString::number(QCoreApplication::applicationPid()),suite.path());});
    menu->addSeparator();QAction* scan=menu->addAction(QStringLiteral("Rescan visualizers"));QObject::connect(scan,&QAction::triggered,menu,[=]{
        QProcess process;process.start(executable(suite,"VisualizersCtl"),QStringList()<<QStringLiteral("scan")<<QStringLiteral("--suite")<<suite.path());
        if(!process.waitForFinished(10000)){process.kill();process.waitForFinished();QMessageBox::warning(menu,QStringLiteral("Scan failed"),QStringLiteral("The scan did not finish."));return;}
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)QMessageBox::warning(menu,QStringLiteral("Scan failed"),QStringLiteral("Check write access to the visualizers directory."));
    });
}
inline void install(QMenuBar* bar,QAction* before=nullptr){
    QDir suite(suitePath());QMenu* menu=new QMenu(QStringLiteral("Visuali&zers"),bar);if(before)bar->insertMenu(before,menu);else bar->addMenu(menu);
    // Adding a menu must not push Help into Qt's overflow at the default size.
    QTimer::singleShot(0,bar,[bar]{bar->window()->setMinimumWidth(qMax(bar->window()->minimumWidth(),bar->sizeHint().width()));});
    QObject::connect(menu,&QMenu::aboutToShow,menu,[=]{populate(menu,suite);});
    if(qEnvironmentVariableIsSet("VLC_VISUALIZERS_SELF_TEST"))QTimer::singleShot(1200,menu,[=]{
        populate(menu,suite);QJsonArray order;for(QAction* action:bar->actions()){QString title=action->text();title.remove(QLatin1Char('&'));order.append(title);}
        QJsonObject result;result.insert(QStringLiteral("attached"),true);result.insert(QStringLiteral("integration"),QStringLiteral("qt-inline"));result.insert(QStringLiteral("entries"),cache(suite).value(QStringLiteral("visualizers")).toArray().size());result.insert(QStringLiteral("menu_order"),order);
        QFile report(suite.filePath(QStringLiteral("menu-test.json")));if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(result).toJson());
        bar->window()->grab().save(suite.filePath(QStringLiteral("vlc-menu-test.png")));
        const QString wanted=qEnvironmentVariable("VLC_VISUALIZERS_TEST_RENDERER");QJsonArray items=cache(suite).value(QStringLiteral("visualizers")).toArray();int selected=0;
        for(int n=0;n<items.size();++n)if(items[n].toObject().value(QStringLiteral("id")).toString()==wanted)selected=n;
        if(selected<menu->actions().size()&&menu->actions()[selected]->menu())menu->actions()[selected]->menu()->actions().first()->trigger();
    });
}
}
