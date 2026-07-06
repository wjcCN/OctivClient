#pragma once

#include <QChar>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace Octiv {

struct DeviceInfo
{
    QString serialNumber;
    QString sensorType;
    QVector<double> frequencies;
    QVector<int> harmonics;
    QString firmware;
    QString firmwareRev;
    QString fpgaRev;
    QString delayClock;
    QString calibrationDate;
};

struct DeviceConfig
{
    int refreshRate = 1000;
    QVector<int> selectedHarmonics;
    QChar signalLock = QLatin1Char('V');
};

struct Temperature
{
    double pcbTemperature = 0.0;
    double sensorTemperature = 0.0;
};

struct ChannelData
{
    int channel = 0;
    qint64 timestamp = 0;
    double frequency = 0.0;
    double voltage = 0.0;
    double current = 0.0;
    double phase = 0.0;
};

struct IonFluxParams
{
    double voltageDrop = 0.0;
    double seriesResistance = 0.0;
    double electrodeArea = 1.0;
};

} // namespace Octiv
