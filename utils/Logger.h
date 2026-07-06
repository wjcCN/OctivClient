#pragma once

#include <QObject>
#include <QString>

class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger &instance();

    static void info(const QString &message);
    static void warning(const QString &message);
    static void error(const QString &message);
    static void http(const QString &message);

signals:
    void logMessage(const QString &message);

private:
    explicit Logger(QObject *parent = nullptr);

    void write(const QString &level, const QString &message);
};
