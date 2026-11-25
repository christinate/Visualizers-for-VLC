#include "settings.hpp"
#include <QApplication>
#include <QDialog>
#include <QFormLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QLabel>
bool native::editSettings(Settings& value,const std::string& title,int argc,char** argv){
    QApplication app(argc,argv);QDialog dialog;dialog.setWindowTitle(QString::fromStdString(title)+QStringLiteral(" settings"));QFormLayout layout(&dialog);
    QCheckBox full(QStringLiteral("Always open visualizer in full screen"));full.setChecked(value.fullscreen);layout.addRow(&full);
    QSpinBox width,height,fps;width.setRange(320,3840);height.setRange(240,2160);fps.setRange(15,144);width.setValue(value.width);height.setValue(value.height);fps.setValue(value.fps);layout.addRow(QStringLiteral("Window width"),&width);layout.addRow(QStringLiteral("Window height"),&height);layout.addRow(QStringLiteral("Frame limit"),&fps);
    QLabel hdr(QStringLiteral("This OpenGL backend uses SDR output.\nWindows HDR settings apply to its Direct3D presenter."));layout.addRow(&hdr);
    QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout.addRow(&buttons);QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return false;value.width=width.value();value.height=height.value();value.fps=fps.value();value.fullscreen=full.isChecked();return true;
}
