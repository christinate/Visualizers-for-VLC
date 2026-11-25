// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings_ui.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTimer>
#include <exception>

namespace visualizers {
std::optional<SettingsJson> editNativeSettings(const SettingsJson& values,const std::string& id,const std::string& title,
        const std::function<bool()>& keepOpen,const SettingsAutomation* automation,const std::string& notice){
    if(!qobject_cast<QApplication*>(QCoreApplication::instance()))throw std::runtime_error("Create NativeUiSession before opening settings");
    const auto fields=settingsFields(id,values.contains("sceneIni"));if(automation)validateSettingsPatch(fields,automation->edits);
    QDialog dialog;dialog.setWindowTitle(QString::fromStdString(title)+QStringLiteral(" settings"));QFormLayout layout(&dialog);
    struct Control{SettingField field;QWidget* widget;SettingsJson visible;};std::vector<Control> controls;
    for(const auto& field:fields){QWidget* widget=nullptr;SettingsJson visible;const auto& value=values.at(field.key);
        if(field.type==SettingType::Boolean){auto* box=new QCheckBox(QString::fromStdString(field.label),&dialog);box->setChecked(value.get<bool>());widget=box;visible=box->isChecked();layout.addRow(box);}
        else if(field.type==SettingType::Ini){
            auto* label=new QLabel(QString::fromStdString(field.label),&dialog);
            label->setTextFormat(Qt::PlainText);
            auto* editor=new QPlainTextEdit(QString::fromStdString(value.get<std::string>()),&dialog);
            editor->setMinimumHeight(160);
            layout.addRow(label);
            layout.addRow(editor);
            widget=editor;
            visible=editor->toPlainText().toStdString();
        }
        else{auto* label=new QLabel(QString::fromStdString(field.label),&dialog);label->setTextFormat(Qt::PlainText);
            if(field.type==SettingType::Choice){auto* box=new QComboBox(&dialog);int selected=0;for(size_t n=0;n<field.choices.size();++n){box->addItem(QString::fromStdString(field.choices[n].label));if(field.choices[n].value==value)selected=int(n);}box->setCurrentIndex(selected);widget=box;visible=field.choices[size_t(selected)].value;}
            else if(field.type==SettingType::Integer){auto* spin=new QSpinBox(&dialog);spin->setRange(int(field.minimum),int(field.maximum));spin->setValue(value.get<int>());widget=spin;visible=spin->value();}
            else{auto* spin=new QDoubleSpinBox(&dialog);spin->setDecimals(9);spin->setRange(field.minimum,field.maximum);spin->setValue(value.get<double>());widget=spin;visible=spin->value();}
            layout.addRow(label,widget);
        }
        controls.push_back({field,widget,visible});
    }
    QLabel note(QString::fromStdString(notice.empty()?settingsNote():notice));note.setTextFormat(Qt::PlainText);note.setWordWrap(true);note.setMaximumWidth(540);layout.addRow(&note);
    QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout.addRow(&buttons);std::optional<SettingsJson> result;
    auto valueOf=[](const Control& control)->SettingsJson{if(control.field.type==SettingType::Boolean)return static_cast<QCheckBox*>(control.widget)->isChecked();if(control.field.type==SettingType::Choice){const auto n=static_cast<QComboBox*>(control.widget)->currentIndex();return control.field.choices.at(size_t(n)).value;}if(control.field.type==SettingType::Integer)return static_cast<QSpinBox*>(control.widget)->value();if(control.field.type==SettingType::Ini)return static_cast<QPlainTextEdit*>(control.widget)->toPlainText().toStdString();return static_cast<QDoubleSpinBox*>(control.widget)->value();};
    QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{try{auto patch=SettingsJson::object();for(const auto& control:controls){const auto value=valueOf(control);if(value!=control.visible)patch[control.field.key]=value;}validateSettingsPatch(fields,patch);result=std::move(patch);dialog.accept();}catch(const std::exception& error){note.setText(QString::fromUtf8(error.what()));}});
    QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    std::exception_ptr failure;unsigned ticks=0;QTimer timer;QObject::connect(&timer,&QTimer::timeout,&dialog,[&]{try{
        if(!keepOpen()){dialog.reject();return;}
        if(automation&&++ticks==2){for(const auto& control:controls){const auto& field=control.field;if(!automation->edits.contains(field.key))continue;const auto& value=automation->edits.at(field.key);
                if(field.type==SettingType::Boolean)static_cast<QCheckBox*>(control.widget)->setChecked(value.get<bool>());
                else if(field.type==SettingType::Choice){for(size_t n=0;n<field.choices.size();++n)if(field.choices[n].value==value)static_cast<QComboBox*>(control.widget)->setCurrentIndex(int(n));}
                else if(field.type==SettingType::Integer)static_cast<QSpinBox*>(control.widget)->setValue(value.get<int>());
                else if(field.type==SettingType::Ini)static_cast<QPlainTextEdit*>(control.widget)->setPlainText(QString::fromStdString(value.get<std::string>()));else static_cast<QDoubleSpinBox*>(control.widget)->setValue(value.get<double>());}
            buttons.button(automation->cancel?QDialogButtonBox::Cancel:QDialogButtonBox::Save)->click();}
    }catch(...){failure=std::current_exception();dialog.reject();}});
    timer.start(50);const auto response=dialog.exec();timer.stop();if(failure)std::rethrow_exception(failure);if(response!=QDialog::Accepted)return {};return result;
}
}
