#pragma once

#include "model/OctivData.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

class JsonParser
{
public:
    static bool parseInfo(const QByteArray &payload, Octiv::DeviceInfo *info, QString *error);
    static bool parseConfig(const QByteArray &payload, Octiv::DeviceConfig *config, QString *error);
    static bool parseTemperature(const QByteArray &payload, Octiv::Temperature *temperature, QString *error);
    static bool parseData(const QByteArray &payload, QVector<Octiv::ChannelData> *channels, QString *error);
    static bool parseIonFluxParams(const QByteArray &payload, Octiv::IonFluxParams *params, QString *error);

private:
    static bool parseRootObject(const QByteArray &payload, QJsonObject *object, QString *error);
};
