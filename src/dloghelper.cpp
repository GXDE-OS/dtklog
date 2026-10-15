// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dloghelper.h"
#include "Logger.h"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <iostream>

DLOG_CORE_BEGIN_NAMESPACE

// Holds where and how a message is logged. It is copied into timing
// callbacks, so it must stay a plain value type.
struct DLogSite
{
    const char *file = nullptr;
    const char *function = nullptr;
    const char *category = nullptr;
    int line = 0;
    Logger::LogLevel level = Logger::LogLevel::Debug;
};

// The private data lives in a child object instead of a QObjectPrivate
// subclass, so only public Qt API is used. DLogHelper is created on the
// caller's stack by the logging macros, so its size must stay the same as
// QObject and a new data member cannot be added.
class DLogHelperPrivate : public QObject
{
public:
    DLogHelperPrivate(const DLogSite &s, QObject *parent)
        : QObject(parent)
        , site(s)
    {
    }

    const DLogSite site;
};

// The data object is created in the constructor as the first child.
static const DLogSite &siteOf(const DLogHelper *helper)
{
    return static_cast<const DLogHelperPrivate *>(helper->children().constFirst())->site;
}

static void writeSite(const DLogSite &site, const QString &msg)
{
    Logger::globalInstance()->write(site.level, site.file, site.line,
                                    site.function, site.category, msg);
}

DLogHelper::DLogHelper(Logger::LogLevel level, const QMessageLogContext &context, QObject *parent)
    : QObject(parent)
{
    DLogSite site;
    site.file = context.file;
    site.line = context.line;
    site.function = context.function;
    site.category = context.category;
    site.level = level;
    new DLogHelperPrivate(site, this);
}

DLogHelper::~DLogHelper()
{
}

void DLogHelper::write(const char *msg, ...)
{
    QString message;
    va_list va;
    va_start(va, msg);
    message = QString::vasprintf(msg, va);
    va_end(va);

    write(message);
}

void DLogHelper::write(const QString &msg)
{
    writeSite(siteOf(this), msg);
}

QDebug DLogHelper::write()
{
    const DLogSite &site = siteOf(this);
    return Logger::globalInstance()->write(site.level, site.file, site.line,
                                           site.function, site.category);
}

void DLogHelper::timing(const QString &msg, QObject *context /* = nullptr*/)
{
    if (!context)
        context = this;

    QElapsedTimer *elapsedTimer = new QElapsedTimer;
    elapsedTimer->start();
    // Copy the site: the helper may already be gone when context is destroyed.
    const DLogSite site = siteOf(this);
    QObject::connect(context, &QObject::destroyed, [elapsedTimer, msg, site]()
                     {
        QString message;
        message = msg + (QLatin1String(" finished in "));

        qint64 elapsed = elapsedTimer->elapsed();
        delete elapsedTimer;

        if (elapsed >= 10000)
            message += QString::number(elapsed / 1000) + QLatin1String("s.");
        else
            message += QString::number(elapsed) + QLatin1String("ms.");

        writeSite(site, message); });
}

Logger::LogLevel DLogHelper::levelFromQtMsgType(QtMsgType mt)
{
    Logger::LogLevel level;
    switch (mt)
    {
    case QtDebugMsg:
        level = Logger::Debug;
        break;
#if QT_VERSION >= 0x050500
    case QtInfoMsg:
        level = Logger::Info;
        break;
#endif
    case QtWarningMsg:
        level = Logger::Warning;
        break;
    case QtCriticalMsg:
        level = Logger::Error;
        break;
    case QtFatalMsg:
        level = Logger::Fatal;
        break;
    }
    return level;
}

QtMsgType DLogHelper::qtMsgTypeFromLogLevel(Logger::LogLevel lvl)
{
    QtMsgType mt;
    switch (lvl)
    {
    case Logger::Debug:
        mt = QtDebugMsg;
        break;
#if QT_VERSION >= 0x050500
    case Logger::Info:
        mt = QtInfoMsg;
        break;
#endif
    case Logger::Warning:
        mt = QtWarningMsg;
        break;
    case Logger::Error:
        mt = QtCriticalMsg;
        break;
    case Logger::Fatal:
        mt = QtFatalMsg;
        break;
    default:
        mt = QtWarningMsg;
    }
    return mt;
}

DLOG_CORE_END_NAMESPACE
