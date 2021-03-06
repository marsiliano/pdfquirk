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

#ifndef PDFCREATOR_H
#define PDFCREATOR_H

#include <QObject>
#include <QProcess>

class QProcess;

class Executor : public QObject
{
    Q_OBJECT
public:
    explicit Executor(QObject *parent = nullptr);

    void buildPdf(const QStringList& files);
    bool scan(bool colorMode);

    void setOutputFile(const QString& fileName);
    QString outputFile();

    void setCommand(const QString& cmd);

public slots:
    void stop();

signals:
    void finished(bool);
    void error(const QString&);

private slots:
    void slotFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString _outputFile;
    QString _cmd;
    QProcess *_process {nullptr};
};

#endif // PDFCREATOR_H
