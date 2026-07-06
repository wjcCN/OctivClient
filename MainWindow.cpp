#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "parser/JsonParser.h"
#include "utils/Logger.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTableWidgetItem>

#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_client(new OctivClient(this)),
      m_dataTimer(new QTimer(this))
{
    ui->setupUi(this);
    setupTable();

    m_dataTimer->setTimerType(Qt::PreciseTimer);
    m_dataTimer->setInterval(ui->refreshRateSpinBox->value());

    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->getInfoButton, &QPushButton::clicked, this, &MainWindow::onGetInfoClicked);
    connect(ui->startStopButton, &QPushButton::clicked, this, &MainWindow::onStartStopClicked);
    connect(ui->getConfigButton, &QPushButton::clicked, this, &MainWindow::onGetConfigClicked);
    connect(ui->applyConfigButton, &QPushButton::clicked, this, &MainWindow::onApplyConfigClicked);
    connect(m_dataTimer, &QTimer::timeout, this, &MainWindow::pollRealtimeData);

    connect(m_client, &OctivClient::rawResponse, this, &MainWindow::handleRawResponse);
    connect(m_client, &OctivClient::requestFailed, this, &MainWindow::handleRequestFailed);
    connect(m_client, &OctivClient::connectionStateChanged, this, &MainWindow::handleConnectionStateChanged);
    connect(&Logger::instance(), &Logger::logMessage, this, &MainWindow::appendLog);

    Logger::info(QStringLiteral("Octiv Industrial Sensor WebService Client ready."));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupTable()
{
    ui->channelTable->setRowCount(5);
    ui->channelTable->setColumnCount(4);
    ui->channelTable->setHorizontalHeaderLabels({
        QStringLiteral("frequency"),
        QStringLiteral("voltage"),
        QStringLiteral("current"),
        QStringLiteral("phase")
    });
    ui->channelTable->setVerticalHeaderLabels({
        QStringLiteral("CH1"),
        QStringLiteral("CH2"),
        QStringLiteral("CH3"),
        QStringLiteral("CH4"),
        QStringLiteral("CH5")
    });
    ui->channelTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->channelTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->channelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->channelTable->setSelectionMode(QAbstractItemView::NoSelection);

    for (int row = 0; row < ui->channelTable->rowCount(); ++row) {
        for (int column = 0; column < ui->channelTable->columnCount(); ++column) {
            ui->channelTable->setItem(row, column, new QTableWidgetItem(QStringLiteral("--")));
        }
    }
}

void MainWindow::onConnectClicked()
{
    m_client->setHost(ui->ipLineEdit->text());
    Logger::info(QStringLiteral("Connecting to Octiv sensor at %1.").arg(m_client->host()));
    m_client->connectToDevice();
    m_client->getConfig();
    m_client->getTemperature();
}

void MainWindow::onGetInfoClicked()
{
    m_client->setHost(ui->ipLineEdit->text());
    m_client->getInfo();
}

void MainWindow::onStartStopClicked()
{
    if (m_dataTimer->isActive()) {
        m_dataTimer->stop();
        ui->startStopButton->setText(QStringLiteral("Start Data"));
        Logger::info(QStringLiteral("Realtime polling stopped."));
        return;
    }

    m_client->setHost(ui->ipLineEdit->text());
    m_dataTimer->setInterval(ui->refreshRateSpinBox->value());
    m_temperaturePollCounter = 0;
    pollRealtimeData();
    m_dataTimer->start();
    ui->startStopButton->setText(QStringLiteral("Stop Data"));
    Logger::info(QStringLiteral("Realtime polling started, interval %1 ms.").arg(m_dataTimer->interval()));
}

void MainWindow::onGetConfigClicked()
{
    m_client->setHost(ui->ipLineEdit->text());
    m_client->getConfig();
}

void MainWindow::onApplyConfigClicked()
{
    const Octiv::DeviceConfig config = configFromUi();
    if (config.selectedHarmonics.isEmpty()) {
        Logger::error(QStringLiteral("Config rejected: selected_harmonics cannot be empty."));
        return;
    }

    m_client->setHost(ui->ipLineEdit->text());
    m_client->postConfig(config);
}

void MainWindow::pollRealtimeData()
{
    m_client->getData();

    ++m_temperaturePollCounter;
    if (m_temperaturePollCounter == 1 || m_temperaturePollCounter >= 5) {
        m_client->getTemperature();
        m_temperaturePollCounter = 1;
    }
}

void MainWindow::handleRawResponse(OctivClient::RequestKind kind, const QByteArray &payload, int httpStatus)
{
    Q_UNUSED(httpStatus)

    QString parseError;
    switch (kind) {
    case OctivClient::RequestKind::Info:
    case OctivClient::RequestKind::ReconnectProbe: {
        Octiv::DeviceInfo info;
        if (JsonParser::parseInfo(payload, &info, &parseError)) {
            updateDeviceInfo(info);
            Logger::info(QStringLiteral("info.cgi parsed successfully."));
        } else {
            Logger::error(QStringLiteral("info.cgi parse failed: %1").arg(parseError));
        }
        break;
    }
    case OctivClient::RequestKind::GetConfig: {
        Octiv::DeviceConfig config;
        if (JsonParser::parseConfig(payload, &config, &parseError)) {
            updateConfig(config);
            Logger::info(QStringLiteral("config.cgi parsed successfully."));
        } else {
            Logger::error(QStringLiteral("config.cgi parse failed: %1").arg(parseError));
        }
        break;
    }
    case OctivClient::RequestKind::PostConfig:
        Logger::info(QStringLiteral("config.cgi accepted by device."));
        m_client->getConfig();
        break;
    case OctivClient::RequestKind::Temperature: {
        Octiv::Temperature temperature;
        if (JsonParser::parseTemperature(payload, &temperature, &parseError)) {
            updateTemperature(temperature);
            Logger::info(QStringLiteral("temperature.cgi parsed successfully."));
        } else {
            Logger::error(QStringLiteral("temperature.cgi parse failed: %1").arg(parseError));
        }
        break;
    }
    case OctivClient::RequestKind::Data: {
        QVector<Octiv::ChannelData> channels;
        if (JsonParser::parseData(payload, &channels, &parseError)) {
            updateChannels(channels);
            Logger::info(QStringLiteral("data.cgi parsed successfully, %1 channel values.").arg(channels.size()));
        } else {
            Logger::error(QStringLiteral("data.cgi parse failed: %1").arg(parseError));
        }
        break;
    }
    case OctivClient::RequestKind::GetIonFluxParams: {
        Octiv::IonFluxParams params;
        if (JsonParser::parseIonFluxParams(payload, &params, &parseError)) {
            Logger::info(QStringLiteral("ionfluxparams.cgi parsed successfully."));
        } else {
            Logger::error(QStringLiteral("ionfluxparams.cgi parse failed: %1").arg(parseError));
        }
        break;
    }
    case OctivClient::RequestKind::PostIonFluxParams:
        Logger::info(QStringLiteral("ionfluxparams.cgi accepted by device."));
        break;
    }
}

void MainWindow::handleRequestFailed(OctivClient::RequestKind kind, int httpStatus, const QString &statusText, const QByteArray &payload)
{
    const QString body = QString::fromUtf8(payload).left(512);
    Logger::error(QStringLiteral("%1 failed: status=%2, message=%3, body=%4")
                      .arg(requestKindName(kind))
                      .arg(httpStatus)
                      .arg(statusText, body.isEmpty() ? QStringLiteral("<empty>") : body));
}

void MainWindow::handleConnectionStateChanged(bool connected)
{
    statusBar()->showMessage(connected ? QStringLiteral("Connected") : QStringLiteral("Disconnected"));
}

void MainWindow::appendLog(const QString &message)
{
    ui->logTextEdit->append(message);
}

void MainWindow::updateDeviceInfo(const Octiv::DeviceInfo &info)
{
    ui->serialNumberValue->setText(info.serialNumber.isEmpty() ? QStringLiteral("--") : info.serialNumber);
    ui->sensorTypeValue->setText(info.sensorType.isEmpty() ? QStringLiteral("--") : info.sensorType);

    const QString firmware = info.firmwareRev.isEmpty()
        ? info.firmware
        : QStringLiteral("%1 / %2").arg(info.firmware, info.firmwareRev);
    ui->firmwareValue->setText(firmware.trimmed().isEmpty() ? QStringLiteral("--") : firmware);
    ui->fpgaValue->setText(info.fpgaRev.isEmpty() ? QStringLiteral("--") : info.fpgaRev);
    ui->calibrationValue->setText(info.calibrationDate.isEmpty() ? QStringLiteral("--") : info.calibrationDate);
}

void MainWindow::updateConfig(const Octiv::DeviceConfig &config)
{
    ui->refreshRateSpinBox->setValue(config.refreshRate);
    m_dataTimer->setInterval(config.refreshRate);

    QStringList harmonics;
    for (const int harmonic : config.selectedHarmonics) {
        harmonics.append(QString::number(harmonic));
    }
    ui->harmonicsLineEdit->setText(harmonics.join(QStringLiteral(",")));

    const int lockIndex = ui->signalLockComboBox->findText(QString(config.signalLock));
    if (lockIndex >= 0) {
        ui->signalLockComboBox->setCurrentIndex(lockIndex);
    }
}

void MainWindow::updateTemperature(const Octiv::Temperature &temperature)
{
    ui->pcbTempValue->setText(QStringLiteral("%1 C").arg(numberText(temperature.pcbTemperature, 2)));
    ui->sensorTempValue->setText(QStringLiteral("%1 C").arg(numberText(temperature.sensorTemperature, 2)));
}

void MainWindow::updateChannels(const QVector<Octiv::ChannelData> &channels)
{
    for (int row = 0; row < ui->channelTable->rowCount(); ++row) {
        if (row >= channels.size()) {
            for (int column = 0; column < ui->channelTable->columnCount(); ++column) {
                ui->channelTable->item(row, column)->setText(QStringLiteral("--"));
            }
            continue;
        }

        const Octiv::ChannelData &channel = channels.at(row);
        ui->channelTable->item(row, 0)->setText(numberText(channel.frequency, 3));
        ui->channelTable->item(row, 1)->setText(numberText(channel.voltage, 3));
        ui->channelTable->item(row, 2)->setText(numberText(channel.current, 3));
        ui->channelTable->item(row, 3)->setText(numberText(channel.phase, 3));
    }
}

Octiv::DeviceConfig MainWindow::configFromUi() const
{
    Octiv::DeviceConfig config;
    config.refreshRate = ui->refreshRateSpinBox->value();
    config.selectedHarmonics = selectedHarmonicsFromUi();
    config.signalLock = ui->signalLockComboBox->currentText().isEmpty()
        ? QLatin1Char('V')
        : ui->signalLockComboBox->currentText().at(0);
    return config;
}

QVector<int> MainWindow::selectedHarmonicsFromUi() const
{
    QVector<int> harmonics;
    const QStringList parts = ui->harmonicsLineEdit->text().split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        if (ok) {
            harmonics.append(value);
        }
    }
    return harmonics;
}

QString MainWindow::requestKindName(OctivClient::RequestKind kind) const
{
    switch (kind) {
    case OctivClient::RequestKind::Info:
        return QStringLiteral("info.cgi");
    case OctivClient::RequestKind::GetConfig:
        return QStringLiteral("config.cgi GET");
    case OctivClient::RequestKind::PostConfig:
        return QStringLiteral("config.cgi POST");
    case OctivClient::RequestKind::Temperature:
        return QStringLiteral("temperature.cgi");
    case OctivClient::RequestKind::Data:
        return QStringLiteral("data.cgi");
    case OctivClient::RequestKind::GetIonFluxParams:
        return QStringLiteral("ionfluxparams.cgi GET");
    case OctivClient::RequestKind::PostIonFluxParams:
        return QStringLiteral("ionfluxparams.cgi POST");
    case OctivClient::RequestKind::ReconnectProbe:
        return QStringLiteral("reconnect probe");
    }

    return QStringLiteral("unknown request");
}

QString MainWindow::numberText(double value, int precision) const
{
    if (!std::isfinite(value)) {
        return QStringLiteral("--");
    }
    return QString::number(value, 'f', precision);
}
