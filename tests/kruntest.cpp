/*
 *  Copyright (C) 2002 David Faure   <faure@kde.org>
 *  Copyright (C) 2003 Waldo Bastian <bastian@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "kruntest.h"

#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegate>

#include <QLabel>
#include <QApplication>
#include <QDebug>
#include <kservice.h>
#include <QPushButton>
#include <QLayout>
#include <QTest> // QFINDTESTDATA

#include <qplatformdefs.h>

const int MAXKRUNS = 100;

testKRun *myArray[MAXKRUNS];

void testKRun::foundMimeType(const QString &_type)
{
    qDebug() << "found mime type" << _type << "for URL=" << url();
    setFinished(true);
    return;
}

static const char testFile[] = "kruntest.cpp";

static const struct {
    const char *text;
    const char *expectedResult;
    const char *exec;
    const char *url;
} s_tests[] = {
    { "run(kwrite, no url)", "should work normally", "kwrite", nullptr },
    { "run(kwrite, file url)", "should work normally", "kwrite", testFile },
    { "run(kwrite, remote url)", "should work normally", "kwrite", "http://www.kde.org" },
    { "run(doesnotexit, no url)", "should show error message", "doesnotexist", nullptr },
    { "run(doesnotexit, file url)", "should show error message", "doesnotexist", testFile },
    { "run(doesnotexit, remote url)", "should use kioexec and show error message", "doesnotexist", "http://www.kde.org" },
    { "run(not-executable-desktopfile)", "should ask for confirmation", "nonexec", nullptr },
    { "run(missing lib, no url)", "should show error message (remove libqca-qt5.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool-qt5", nullptr },
    { "run(missing lib, file url)", "should show error message (remove libqca-qt5.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool-qt5", testFile },
    { "run(missing lib, remote url)", "should show error message (remove libqca-qt5.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool-qt5", "http://www.kde.org" },
};

Receiver::Receiver()
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    QPushButton *h = new QPushButton(QStringLiteral("Press here to terminate"), this);
    lay->addWidget(h);
    connect(h, SIGNAL(clicked()), qApp, SLOT(quit()));

    start = new QPushButton(QStringLiteral("Launch KRuns"), this);
    lay->addWidget(start);
    connect(start, &QAbstractButton::clicked, this, &Receiver::slotStart);

    stop = new QPushButton(QStringLiteral("Stop those KRuns"), this);
    stop->setEnabled(false);
    lay->addWidget(stop);
    connect(stop, &QAbstractButton::clicked, this, &Receiver::slotStop);

    QPushButton *launchOne = new QPushButton(QStringLiteral("Launch one http KRun"), this);
    lay->addWidget(launchOne);
    connect(launchOne, &QAbstractButton::clicked, this, &Receiver::slotLaunchOne);

    for (uint i = 0; i < sizeof(s_tests) / sizeof(*s_tests); ++i) {
        QHBoxLayout *hbox = new QHBoxLayout;
        lay->addLayout(hbox);
        QPushButton *button = new QPushButton(s_tests[i].text, this);
        button->setProperty("testNumber", i);
        hbox->addWidget(button);
        QLabel *label = new QLabel(s_tests[i].expectedResult, this);
        hbox->addWidget(label);
        connect(button, &QAbstractButton::clicked, this, &Receiver::slotLaunchTest);
        hbox->addStretch();
    }

    adjustSize();
    show();
}

void Receiver::slotLaunchTest()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    Q_ASSERT(button);
    const int testNumber = button->property("testNumber").toInt();
    QList<QUrl> urls;
    if (s_tests[testNumber].url) {
        QString urlStr(s_tests[testNumber].url);
        if (urlStr == QLatin1String(testFile)) {
            urlStr = QFINDTESTDATA(testFile);
        }
        urls << QUrl::fromUserInput(urlStr);
    }
    KService::Ptr service;
    if (QByteArray(s_tests[testNumber].exec) == "nonexec") {
        const QString desktopFile = QFINDTESTDATA("../src/ioslaves/trash/kcmtrash.desktop");
        if (desktopFile.isEmpty()) {
            qWarning() << "kcmtrash.desktop not found!";
        }
        const QString dest = QStringLiteral("kcmtrash.desktop");
        QFile::remove(dest);
        bool ok = QFile::copy(desktopFile, dest);
        if (!ok) {
            qWarning() << "Failed to copy" << desktopFile << "to" << dest;
        }
        service = KService::Ptr(new KService(QDir::currentPath() + QLatin1Char('/') + dest));
    } else {
        service = KService::Ptr(new KService("Some Name", s_tests[testNumber].exec, QString()));
    }
    auto *job = new KIO::ApplicationLauncherJob(service, this);
    job->setUrls(urls);
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
}

void Receiver::slotStop()
{
    for (int i = 0; i < MAXKRUNS; i++) {
        qDebug() << "deleting krun" << i;
        delete myArray[i];
    }
    start->setEnabled(true);
    stop->setEnabled(false);
}

void Receiver::slotStart()
{
    for (int i = 0; i < MAXKRUNS; i++) {
        qDebug() << "creating testKRun" << i;
        myArray[i] = new testKRun(QUrl::fromLocalFile(QStringLiteral("file:///tmp")), window());
        myArray[i]->setAutoDelete(false);
    }
    start->setEnabled(false);
    stop->setEnabled(true);
}

void Receiver::slotLaunchOne()
{
    new testKRun(QUrl(QStringLiteral("http://www.kde.org")), window());
}

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("kruntest"));
    QApplication app(argc, argv);

    Receiver receiver;
    return app.exec();
}

