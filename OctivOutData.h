#pragma once

#include <QFile>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QVector>
#include <QtWidgets/QMainWindow>
#include "ui_OctivOutData.h"

class OctivOutData : public QMainWindow
{
    Q_OBJECT

public:
    OctivOutData(QWidget *parent = nullptr);
    ~OctivOutData();

private:
    Ui::OctivOutDataClass ui;
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QFile m_file;
    QVector<int> m_harmonics;

    void initUi();
    void start();
    void stop();
    void pushHarmonic();
    void switchSamplingRate();
    void readData();
    void handleData(const QByteArray &payload);
    QString outDirPath() const;
    double selectedFrequency() const;
    int selectedSamplingRate() const;
    QString jsonText(const QJsonValue &value) const;
    QString arrayText(const QJsonValue &value, int index) const;
};
