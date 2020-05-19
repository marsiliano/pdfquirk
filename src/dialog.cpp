/*
  This file is part of pdfquirk.
  Copyright 2020, Klaas Freitag <kraft@freisturz.de>

  pdfquirk is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  pdfquirk is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with pdfquirk.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dialog.h"
#include "./ui_dialog.h"

#include <QFileDialog>
#include <QDir>
#include <QTimer>
#include <QEventLoop>
#include <QDialogButtonBox>
#include <QSettings>
#include <QDir>
#include <QObject>
#include <QResizeEvent>
#include <QDebug>
#include <QtGlobal>
#include <QScrollBar>

#include "imagelistdelegate.h"
#include "executor.h"



bool SizeCatcher::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        QResizeEvent *sizeEvent = static_cast<QResizeEvent *>(event);
        int h = sizeEvent->size().height();
        qDebug() << "caught height" << h;
        const QSize s(qRound(h/1.41), h);
        emit thumbSize(s);
    }
    return QObject::eventFilter(obj, event);
}

// ==================================================================

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    // Prepare a settings file
    const QString configHome { QString("%1/.config/pdfquirkrc").arg(QDir::homePath()) };
    _settings.reset(new QSettings(configHome, QSettings::IniFormat));

    // Thumbnail sizes
    int thumbWidth = 100;
    int thumbHeight = 141;

    ui->setupUi(this);

    ui->listviewThumbs->setModel(&_model);
    _delegate = new ImageListDelegate();
    ui->listviewThumbs->setItemDelegate(_delegate);
    ui->listviewThumbs->setSelectionMode(QAbstractItemView::NoSelection);
    ui->listviewThumbs->setResizeMode(QListView::Adjust);
    ui->listviewThumbs->setMaximumHeight(300);
    ui->listviewThumbs->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);

    QFont f = ui->listviewThumbs->font();
    QFontMetrics fm(f);
    thumbWidth += 4;
    thumbHeight += 4 + fm.height() + 2;
    // ui->listviewThumbs->setFixedHeight(thumbHeight+6);

    _delegate->slotThumbSize(QSize(thumbWidth, thumbHeight));

    // size catcher
    SizeCatcher *sizeCatcher = new SizeCatcher;
    connect(sizeCatcher, &SizeCatcher::thumbSize, this, &Dialog::slotListViewSize);
    ui->listviewThumbs->installEventFilter(sizeCatcher);

    connect (ui->pbAddFromFile, &QPushButton::clicked, this, &Dialog::slotFromFile);
    connect (ui->pbAddFromScanner, &QPushButton::clicked, this, &Dialog::slotFromScanner);

    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &Dialog::slotButtonClicked);

    updateInfoText();
}

void Dialog::slotListViewSize(QSize s)
{
    int spacing = ui->listviewThumbs->contentsMargins().top() + ui->listviewThumbs->contentsMargins().bottom();
    int h = s.height();
    if (ui->listviewThumbs->horizontalScrollBar()->isVisible()) {
        h = s.height() - ui->listviewThumbs->horizontalScrollBar()->height();
    }
    s.setHeight(h - spacing);
    _delegate->slotThumbSize(s);
}

void Dialog::slotButtonClicked(QAbstractButton *button)
{
    if (!button) return;
    const QString path = _settings->value(_SettingsLastFilePath, QDir::homePath()).toString();

    if( button == ui->buttonBox->button(QDialogButtonBox::StandardButton::Save)) {
        QStringList files = _model.files();

        const QString saveFile = QFileDialog::getSaveFileName(this, tr("Save PDF File"), path, "PDF (*.pdf)");
        if (!saveFile.isEmpty()) {
            Executor *creator = new Executor;
            connect(creator, &Executor::finished, this, &Dialog::pdfCreatorFinished);
            creator->setOutputFile(saveFile);
            QApplication::setOverrideCursor(Qt::WaitCursor);
            creator->buildPdf(files);
        }
    }
}

void Dialog::accept()
{
    // do nothing to not close the dialog.
}

void Dialog::pdfCreatorFinished(bool success)
{
    QApplication::restoreOverrideCursor();

    // get the result file name from the creator object.
    QString resultFile;
    Executor *creator = static_cast<Executor*>(sender());
    if (creator) {
        resultFile = creator->outputFile();
        creator->deleteLater();
    }
    if (success) {
        _model.clear();
    }
    updateInfoText(resultFile);
}

void Dialog::slotFromFile()
{
    QString path = _settings->value(_SettingsLastFilePath, QDir::homePath()).toString();

    QStringList files = QFileDialog::getOpenFileNames(this, tr("Add files to PDF"), path, "Images (*.png *.jpeg *.jpg)");

    startLengthyOperation();
    for (const QString& file : files) {
        QFileInfo fi(file);
        qApp->processEvents();
        _model.addImageFile(file);

        path = fi.path();
    }
    endLengthyOperation();
    updateInfoText();
    _settings->setValue(_SettingsLastFilePath, path);
    _settings->sync();
}

void Dialog::updateInfoText(const QString& saveFile)
{
    QString text = tr("No images loaded. Load from scanner or file using the buttons above.");
    if (_model.rowCount() > 0) {
        text = tr("%1 image(s) loaded. Press Save button to create PDF.").arg(_model.rowCount());
    }
    bool openExt { false };

    if (!saveFile.isEmpty()) {
        text = tr("PDF file was saved to <a href=\"file:%1\">%1</a>").arg(saveFile);
        openExt = true;
    }
    ui->labInfo->setOpenExternalLinks(openExt);
    ui->labInfo->setText(text);
    ui->buttonBox->button(QDialogButtonBox::StandardButton::Save)->setEnabled( _model.rowCount() > 0 );
}


void Dialog::slotFromScanner()
{
    QString scanCmd;
    const QString defltCmd{ "scanimage --mode 'Grey' --resolution 150 -l 0 -t 0 -x 210 -y 297 --format png"};

    scanCmd = _settings->value(_SettingsScanBW, defltCmd ).toString();

    Executor *scanner = new Executor;
    scanner->setCommand(scanCmd);

    connect(scanner, &Executor::finished, this, &Dialog::slotScanFinished);
    startLengthyOperation();
    if (!scanner->scan(false))
        slotScanFinished(false);

}

void Dialog::slotScanFinished(bool success)
{

    // get the result file name from the creator object.
    QString resultFile;
    Executor *creator = static_cast<Executor*>(sender());
    if (creator) {
        resultFile = creator->outputFile();
        creator->deleteLater();
    }
    if (success) {
        _model.addImageFile(resultFile);
    }
    endLengthyOperation();

    updateInfoText(resultFile);
}

void Dialog::startLengthyOperation()
{
    if (_lengthyOpRunning) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    _lengthyOpRunning = true;
}

void Dialog::endLengthyOperation()
{
    if (!_lengthyOpRunning) return;
    QApplication::restoreOverrideCursor();
    _lengthyOpRunning = true;
}

Dialog::~Dialog()
{
    delete ui;
}

