#include "parser/JsonParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

QString valueToString(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return QString();
}

int valueToInt(const QJsonValue &value, int fallback = 0)
{
    if (value.isDouble()) {
        return value.toInt(fallback);
    }
    if (value.isString()) {
        bool ok = false;
        const int result = value.toString().toInt(&ok);
        return ok ? result : fallback;
    }
    return fallback;
}

double valueToDouble(const QJsonValue &value, double fallback = 0.0)
{
    if (value.isDouble()) {
        return value.toDouble(fallback);
    }
    if (value.isString()) {
        bool ok = false;
        const double result = value.toString().toDouble(&ok);
        return ok ? result : fallback;
    }
    return fallback;
}

QVector<int> intVector(const QJsonValue &value)
{
    QVector<int> result;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        result.reserve(array.size());
        for (const QJsonValue &entry : array) {
            result.append(valueToInt(entry));
        }
    } else if (!value.isUndefined() && !value.isNull()) {
        result.append(valueToInt(value));
    }
    return result;
}

QVector<double> doubleVector(const QJsonValue &value)
{
    QVector<double> result;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        result.reserve(array.size());
        for (const QJsonValue &entry : array) {
            result.append(valueToDouble(entry, std::numeric_limits<double>::quiet_NaN()));
        }
    } else if (!value.isUndefined() && !value.isNull()) {
        result.append(valueToDouble(value, std::numeric_limits<double>::quiet_NaN()));
    }
    return result;
}

double vectorValue(const QVector<double> &values, int index)
{
    if (index >= 0 && index < values.size()) {
        return values.at(index);
    }
    return std::numeric_limits<double>::quiet_NaN();
}

QString missingObjectError()
{
    return QStringLiteral("JSON root must be an object.");
}

} // namespace

bool JsonParser::parseRootObject(const QByteArray &payload, QJsonObject *object, QString *error)
{
    if (!object) {
        if (error) {
            *error = QStringLiteral("Parser output object is null.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = QStringLiteral("JSON parse error at %1: %2")
                         .arg(parseError.offset)
                         .arg(parseError.errorString());
        }
        return false;
    }

    if (!document.isObject()) {
        if (error) {
            *error = missingObjectError();
        }
        return false;
    }

    *object = document.object();
    return true;
}

bool JsonParser::parseInfo(const QByteArray &payload, Octiv::DeviceInfo *info, QString *error)
{
    QJsonObject object;
    if (!parseRootObject(payload, &object, error)) {
        return false;
    }

    if (!info) {
        if (error) {
            *error = QStringLiteral("DeviceInfo output is null.");
        }
        return false;
    }

    info->serialNumber = valueToString(object.value(QStringLiteral("serial_number")));
    info->sensorType = valueToString(object.value(QStringLiteral("sensor_type")));
    info->frequencies = doubleVector(object.value(QStringLiteral("frequencies")));
    info->harmonics = intVector(object.value(QStringLiteral("harmonics")));
    info->firmware = valueToString(object.value(QStringLiteral("firmware")));
    info->firmwareRev = valueToString(object.value(QStringLiteral("firmware_rev")));
    info->fpgaRev = valueToString(object.value(QStringLiteral("fpga_rev")));
    info->delayClock = valueToString(object.value(QStringLiteral("delay_clock")));
    info->calibrationDate = valueToString(object.value(QStringLiteral("calibration_date")));

    return true;
}

bool JsonParser::parseConfig(const QByteArray &payload, Octiv::DeviceConfig *config, QString *error)
{
    QJsonObject object;
    if (!parseRootObject(payload, &object, error)) {
        return false;
    }

    if (!config) {
        if (error) {
            *error = QStringLiteral("DeviceConfig output is null.");
        }
        return false;
    }

    config->refreshRate = std::max(50, valueToInt(object.value(QStringLiteral("refresh_rate")), config->refreshRate));
    config->selectedHarmonics = intVector(object.value(QStringLiteral("selected_harmonics")));

    const QString lock = valueToString(object.value(QStringLiteral("signal_lock"))).trimmed().toUpper();
    if (lock == QLatin1String("V") || lock == QLatin1String("C")) {
        config->signalLock = lock.at(0);
    }

    return true;
}

bool JsonParser::parseTemperature(const QByteArray &payload, Octiv::Temperature *temperature, QString *error)
{
    QJsonObject object;
    if (!parseRootObject(payload, &object, error)) {
        return false;
    }

    if (!temperature) {
        if (error) {
            *error = QStringLiteral("Temperature output is null.");
        }
        return false;
    }

    temperature->pcbTemperature = valueToDouble(object.value(QStringLiteral("PCB_Temperature")));
    temperature->sensorTemperature = valueToDouble(object.value(QStringLiteral("Sensor_Temperature")));
    return true;
}

bool JsonParser::parseData(const QByteArray &payload, QVector<Octiv::ChannelData> *channels, QString *error)
{
    if (!channels) {
        if (error) {
            *error = QStringLiteral("Channel output is null.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = QStringLiteral("JSON parse error at %1: %2")
                         .arg(parseError.offset)
                         .arg(parseError.errorString());
        }
        return false;
    }

    QJsonArray array;
    if (document.isArray()) {
        array = document.array();
    } else if (document.isObject() && document.object().value(QStringLiteral("data")).isArray()) {
        array = document.object().value(QStringLiteral("data")).toArray();
    } else {
        if (error) {
            *error = QStringLiteral("Data JSON root must be an array or contain a data array.");
        }
        return false;
    }

    QVector<Octiv::ChannelData> parsed;
    for (const QJsonValue &entry : array) {
        if (!entry.isObject()) {
            continue;
        }

        const QJsonObject object = entry.toObject();
        const qint64 timestamp = static_cast<qint64>(valueToDouble(object.value(QStringLiteral("timestamp"))));
        const double frequency = valueToDouble(object.value(QStringLiteral("frequency")));
        const QVector<double> voltages = doubleVector(object.value(QStringLiteral("voltage")));
        const QVector<double> currents = doubleVector(object.value(QStringLiteral("current")));
        const QVector<double> phases = doubleVector(object.value(QStringLiteral("phase")));
        const int itemCount = qMax(qMax(1, voltages.size()), qMax(currents.size(), phases.size()));

        for (int i = 0; i < itemCount; ++i) {
            Octiv::ChannelData channel;
            channel.channel = parsed.size() + 1;
            channel.timestamp = timestamp;
            channel.frequency = frequency;
            channel.voltage = vectorValue(voltages, i);
            channel.current = vectorValue(currents, i);
            channel.phase = vectorValue(phases, i);
            parsed.append(channel);
        }
    }

    *channels = parsed;
    return true;
}

bool JsonParser::parseIonFluxParams(const QByteArray &payload, Octiv::IonFluxParams *params, QString *error)
{
    QJsonObject object;
    if (!parseRootObject(payload, &object, error)) {
        return false;
    }

    if (!params) {
        if (error) {
            *error = QStringLiteral("IonFluxParams output is null.");
        }
        return false;
    }

    params->voltageDrop = valueToDouble(object.value(QStringLiteral("voltage_drop")), params->voltageDrop);
    params->seriesResistance = valueToDouble(object.value(QStringLiteral("series_resistance")), params->seriesResistance);
    params->electrodeArea = valueToDouble(object.value(QStringLiteral("electrode_area")), params->electrodeArea);
    if (qFuzzyIsNull(params->electrodeArea)) {
        params->electrodeArea = 1.0;
    }

    return true;
}
