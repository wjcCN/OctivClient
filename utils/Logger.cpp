#include "utils/Logger.h"

#include <QDateTime>

Logger::Logger(QObject *parent)
    : QObject(parent)
{
}

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::info(const QString &message)
{
    instance().write(QStringLiteral("INFO"), message);
}

void Logger::warning(const QString &message)
{
    instance().write(QStringLiteral("WARN"), message);
}

void Logger::error(const QString &message)
{
    instance().write(QStringLiteral("ERROR"), message);
}

void Logger::http(const QString &message)
{
    instance().write(QStringLiteral("HTTP"), message);
}

void Logger::write(const QString &level, const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    emit logMessage(QStringLiteral("[%1] [%2] %3").arg(timestamp, level, message));
}
