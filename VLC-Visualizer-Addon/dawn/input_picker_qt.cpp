// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_picker.hpp"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <exception>
#include <memory>
#include <stdexcept>

namespace visualizers {
struct NativeUiSession::Impl {
    int argc=1;char name[16]="VLC Visualizers";char* argv[2]={name,nullptr};
    std::unique_ptr<QApplication> application;
    Impl(){
        if(!QApplication::instance()){application=std::make_unique<QApplication>(argc,argv);application->setQuitOnLastWindowClosed(false);}
        else if(!qobject_cast<QApplication*>(QCoreApplication::instance()))throw std::runtime_error("Native UI requires QApplication");
    }
};
NativeUiSession::NativeUiSession():impl_(std::make_unique<Impl>()){}
NativeUiSession::~NativeUiSession()=default;
void pumpNativeUi(){QApplication::processEvents();}
std::optional<std::string> pickAudioInput(const std::vector<InputDevice>& devices,const std::string& current,
        const std::function<bool()>& keepOpen,const PickerAutomation* automation,const std::string& notice){
    if(devices.size()>1024)throw std::runtime_error("Audio input list exceeds its limit");
    if(!qobject_cast<QApplication*>(QCoreApplication::instance()))throw std::runtime_error("Create NativeUiSession before opening a picker");
    QDialog dialog;dialog.setWindowTitle(QStringLiteral("Audio input"));dialog.resize(640,360);
    QVBoxLayout layout(&dialog);QLabel label(notice.empty()?QStringLiteral("Choose a recording input. Ctrl-L opens this list again."):QString::fromUtf8(notice.data(),qsizetype(notice.size())));label.setWordWrap(true);layout.addWidget(&label);
    QListWidget choices;choices.addItem(QStringLiteral("Default recording input"));
    for(const auto& device:devices){const auto label=device.label+(device.isDefault?" (default)":"")+" — "+device.id;
        choices.addItem(QString::fromUtf8(label.data(),qsizetype(label.size())));}
    choices.setCurrentRow(int(selectedInputRow(devices,current)));layout.addWidget(&choices);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons.button(QDialogButtonBox::Ok)->setText(QStringLiteral("Use input"));layout.addWidget(&buttons);
    QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(&choices,&QListWidget::itemDoubleClicked,&dialog,&QDialog::accept);
    std::exception_ptr failure;unsigned ticks=0;QTimer timer;
    QObject::connect(&timer,&QTimer::timeout,&dialog,[&]{try{
        if(!keepOpen()){dialog.reject();return;}
        if(automation&&++ticks==automation->pollsBeforeAction){if(automation->cancel)dialog.reject();else{if(automation->row)choices.setCurrentRow(int(*automation->row));buttons.button(QDialogButtonBox::Ok)->click();}}
    }catch(...){failure=std::current_exception();dialog.reject();}});
    timer.start(50);const auto response=dialog.exec();timer.stop();
    if(failure)std::rethrow_exception(failure);
    if(response!=QDialog::Accepted||choices.currentRow()<0)return std::nullopt;
    return inputRowId(devices,unsigned(choices.currentRow()));
}
}
