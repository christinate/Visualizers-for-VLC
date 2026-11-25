// SPDX-License-Identifier: GPL-2.0-or-later
#include <vlc_common.h>
#include <vlc_interface.h>
#include <QApplication>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <atomic>
#include <thread>
#include <memory>
#include "visualizers_menu.hpp"
extern "C" void playlist_EnableAudioFilter(playlist_t*,const char*,bool);
struct State {std::atomic<bool> stopped{false};std::thread worker;QPointer<QTimer> timer;QPointer<QMenuBar> bar;QPointer<QMenu> menu;};
struct intf_sys_t {std::shared_ptr<State> state;};
extern "C" int NativeMenuOpen(vlc_object_t* object){
 auto intf=reinterpret_cast<intf_thread_t*>(object);auto sys=new(std::nothrow)intf_sys_t;if(!sys)return VLC_ENOMEM;intf->p_sys=sys;sys->state=std::make_shared<State>();auto state=sys->state;
 state->worker=std::thread([intf,state]{playlist_EnableAudioFilter(pl_Get(intf),"vlc_visualizers",true);while(!state->stopped&&!QCoreApplication::instance())std::this_thread::sleep_for(std::chrono::milliseconds(20));if(state->stopped)return;
  QMetaObject::invokeMethod(QCoreApplication::instance(),[state]{if(state->stopped)return;auto timer=new QTimer(QCoreApplication::instance());state->timer=timer;QObject::connect(timer,&QTimer::timeout,timer,[state]{if(state->stopped){state->timer->stop();state->timer->deleteLater();return;}if(state->menu)return;for(QWidget* window:QApplication::topLevelWidgets()){auto main=qobject_cast<QMainWindow*>(window);if(!main||!main->isVisible())continue;auto bar=main->menuBar();auto actions=bar->actions();if(actions.size()<2)continue;InlineVisualizers::install(bar,actions.last());state->bar=bar;state->menu=bar->actions().at(actions.size()-1)->menu();break;}});timer->start(200);},Qt::QueuedConnection);
 });return VLC_SUCCESS;
}
extern "C" void NativeMenuClose(vlc_object_t* object){auto intf=reinterpret_cast<intf_thread_t*>(object);auto sys=intf->p_sys;if(!sys)return;auto state=sys->state;state->stopped=true;if(state->worker.joinable())state->worker.join();if(QCoreApplication::instance())QMetaObject::invokeMethod(QCoreApplication::instance(),[state]{if(state->timer){state->timer->stop();state->timer->deleteLater();}if(state->menu){if(state->bar)state->bar->removeAction(state->menu->menuAction());delete state->menu;}},Qt::QueuedConnection);delete sys;intf->p_sys=nullptr;}
