#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include "core/LogFormat.h"

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::setFilePath(const QString &path)
{
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_file.setFileName(path);
    if (path.isEmpty()) {
        return;
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return; // no directory, no file output; entryLogged still fires
    }
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return; // isOpen() stays false, so log() skips file output
    }
}

void Logger::info(const QString &msg, const QJsonObject &fields)
{
    log(QStringLiteral("info"), msg, fields);
}

void Logger::error(const QString &msg, const QJsonObject &fields)
{
    log(QStringLiteral("error"), msg, fields);
}

void Logger::log(const QString &level, const QString &msg, const QJsonObject &fields)
{
    const QJsonObject entry =
        logformat::makeEntry(level, msg, fields, QDateTime::currentDateTimeUtc());
    if (m_file.isOpen()) {
        m_file.write(logformat::toLine(entry));
        // It's an audit trail: the line must be on disk before whatever it
        // records can be followed by another action.
        m_file.flush();
    }
    emit entryLogged(entry);
}
