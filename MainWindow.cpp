#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "parser/JsonParser.h"
#include "utils/Logger.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QTextStream>

#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_client(new OctivClient(this)),
      m_dataTimer(new QTimer(this))
{
    ui->setupUi(this);//根据ui文件创建所有界面控件
    setupTable();
    setupHarmonicControls();
    setupLanguageControls();
    applyLanguage();

    m_dataTimer->setTimerType(Qt::PreciseTimer);//尽量提高定时轮询精度
    m_dataTimer->setInterval(ui->refreshRateSpinBox->value());//默认使用界面刷新周期输入框的值

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
    endDataRecording();
    delete ui;
}

void MainWindow::setupTable()
{
    ui->channelTable->setRowCount(5);
    ui->channelTable->setColumnCount(5);
    ui->channelTable->setHorizontalHeaderLabels({
        QStringLiteral("timestamp"),
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
    ui->channelTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);//列宽自动填满表格
    ui->channelTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->channelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->channelTable->setSelectionMode(QAbstractItemView::NoSelection);

    for (int row = 0; row < ui->channelTable->rowCount(); ++row) {
        for (int column = 0; column < ui->channelTable->columnCount(); ++column) {
            ui->channelTable->setItem(row, column, new QTableWidgetItem(QStringLiteral("--")));//单元格填充--
        }
    }
}

void MainWindow::setupLanguageControls()
{
    m_languageLabel = new QLabel(this);
    m_languageComboBox = new QComboBox(this);
    m_languageComboBox->addItem(QStringLiteral("中文"), QVariant::fromValue(0));
    m_languageComboBox->addItem(QStringLiteral("English"), QVariant::fromValue(1));
    m_languageComboBox->setCurrentIndex(0);

    const int insertIndex = qMax(0, ui->topControlLayout->count() - 1);
    ui->topControlLayout->insertWidget(insertIndex, m_languageLabel);
    ui->topControlLayout->insertWidget(insertIndex + 1, m_languageComboBox);

    connect(m_languageComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_language = index == 1 ? Language::English : Language::Chinese;
        applyLanguage();
    });
}

void MainWindow::setupHarmonicControls()
{
    ui->harmonicsLineEdit->hide();

    m_harmonicsWidget = new QWidget(this);
    auto *layout = new QHBoxLayout(m_harmonicsWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_harmonicLabels.reserve(5);
    m_harmonicComboBoxes.reserve(5);
    for (int i = 0; i < 5; ++i) {
        auto *label = new QLabel(QStringLiteral("CH%1").arg(i + 1), m_harmonicsWidget);
        auto *comboBox = new QComboBox(m_harmonicsWidget);
        comboBox->setMinimumWidth(56);
        comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        m_harmonicLabels.append(label);
        m_harmonicComboBoxes.append(comboBox);
        layout->addWidget(label);
        layout->addWidget(comboBox);
    }
    layout->addStretch(1);

    ui->configLayout->addWidget(m_harmonicsWidget, 1, 1);
    updateHarmonicOptions({});
    setSelectedHarmonics({0, 0, 0, 0, 0});
}

void MainWindow::applyLanguage()
{
    const bool zh = isChinese();

    setWindowTitle(zh
        ? QStringLiteral("Octiv 工业传感器 WebService 客户端")
        : QStringLiteral("Octiv Industrial Sensor WebService Client"));

    m_languageLabel->setText(zh ? QStringLiteral("语言") : QStringLiteral("Language"));
    m_languageComboBox->setItemText(0, zh ? QStringLiteral("中文") : QStringLiteral("Chinese"));
    m_languageComboBox->setItemText(1, zh ? QStringLiteral("English") : QStringLiteral("English"));

    ui->ipLabel->setText(zh ? QStringLiteral("IP 地址") : QStringLiteral("IP Address"));
    ui->connectButton->setText(zh ? QStringLiteral("连接") : QStringLiteral("Connect"));
    ui->getInfoButton->setText(zh ? QStringLiteral("获取信息") : QStringLiteral("Get Info"));
    ui->startStopButton->setText(m_dataTimer->isActive() ? stopDataText() : startDataText());

    ui->infoGroupBox->setTitle(zh ? QStringLiteral("设备信息") : QStringLiteral("Device Information"));
    ui->serialNumberLabel->setText(zh ? QStringLiteral("序列号") : QStringLiteral("Serial Number"));
    ui->sensorTypeLabel->setText(zh ? QStringLiteral("传感器类型") : QStringLiteral("Sensor Type"));
    ui->firmwareLabel->setText(zh ? QStringLiteral("固件版本") : QStringLiteral("Firmware Version"));
    ui->fpgaLabel->setText(zh ? QStringLiteral("FPGA 版本") : QStringLiteral("FPGA Version"));
    ui->calibrationLabel->setText(zh ? QStringLiteral("校准日期") : QStringLiteral("Calibration Date"));

    ui->configGroupBox->setTitle(zh ? QStringLiteral("设备配置") : QStringLiteral("Device Configuration"));
    ui->refreshRateLabel->setText(zh ? QStringLiteral("刷新周期 (ms)") : QStringLiteral("Refresh Rate (ms)"));
    ui->harmonicsLabel->setText(zh ? QStringLiteral("CH1-CH5 谐波") : QStringLiteral("CH1-CH5 Harmonics"));
    ui->signalLockLabel->setText(zh ? QStringLiteral("信号锁定") : QStringLiteral("Signal Lock"));
    ui->getConfigButton->setText(zh ? QStringLiteral("读取配置") : QStringLiteral("Get Config"));
    ui->applyConfigButton->setText(zh ? QStringLiteral("应用配置") : QStringLiteral("Apply Config"));

    ui->temperatureGroupBox->setTitle(zh ? QStringLiteral("温度") : QStringLiteral("Temperature"));
    ui->pcbTempLabel->setText(zh ? QStringLiteral("PCB 温度") : QStringLiteral("PCB Temperature"));
    ui->sensorTempLabel->setText(zh ? QStringLiteral("传感器温度") : QStringLiteral("Sensor Temperature"));

    ui->dataGroupBox->setTitle(zh ? QStringLiteral("5 通道实时数据") : QStringLiteral("5 Channel Realtime Data"));
    ui->channelTable->setHorizontalHeaderLabels({
        zh ? QStringLiteral("时间戳") : QStringLiteral("timestamp"),
        zh ? QStringLiteral("频率") : QStringLiteral("frequency"),
        zh ? QStringLiteral("电压") : QStringLiteral("voltage"),
        zh ? QStringLiteral("电流") : QStringLiteral("current"),
        zh ? QStringLiteral("相位") : QStringLiteral("phase")
    });

    ui->logGroupBox->setTitle(zh ? QStringLiteral("通信日志") : QStringLiteral("Communication Log"));
}

void MainWindow::onConnectClicked()
{
    m_client->setHost(ui->ipLineEdit->text());
    Logger::info(QStringLiteral("Connecting to Octiv sensor at %1.").arg(m_client->host()));
    m_client->connectToDevice();
    m_client->getInfo();
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
    if (m_dataTimer->isActive()) {//如果正在采集
        m_dataTimer->stop();
        endDataRecording();
        ui->startStopButton->setText(startDataText());
        Logger::info(QStringLiteral("Realtime polling stopped."));
        return;
    }

    m_client->setHost(ui->ipLineEdit->text());
    m_dataTimer->setInterval(ui->refreshRateSpinBox->value());
    if (!beginDataRecording()) {
        return;
    }
    m_temperaturePollCounter = 0;
    pollRealtimeData();
    m_dataTimer->start();
    ui->startStopButton->setText(stopDataText());
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
    m_lastPostedConfig = config;
    m_waitingConfigReadback = true;
    Logger::info(QStringLiteral("Posting config selected_harmonics=[%1], refresh_rate=%2, signal_lock=%3.")
                     .arg(harmonicListText(config.selectedHarmonics))
                     .arg(config.refreshRate)
                     .arg(config.signalLock));
    m_client->postConfig(config);
}

void MainWindow::pollRealtimeData()
{
    m_client->getData();

    ++m_temperaturePollCounter;
    if (m_temperaturePollCounter == 1 || m_temperaturePollCounter >= 5) {//首次和每五次进行温度请求
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
            if (m_waitingConfigReadback) {
                if (config.selectedHarmonics != m_lastPostedConfig.selectedHarmonics
                    || config.refreshRate != m_lastPostedConfig.refreshRate
                    || config.signalLock != m_lastPostedConfig.signalLock) {
                    Logger::warning(QStringLiteral("Device readback differs from posted config. posted selected_harmonics=[%1], refresh_rate=%2, signal_lock=%3; device selected_harmonics=[%4], refresh_rate=%5, signal_lock=%6.")
                                        .arg(harmonicListText(m_lastPostedConfig.selectedHarmonics))
                                        .arg(m_lastPostedConfig.refreshRate)
                                        .arg(m_lastPostedConfig.signalLock)
                                        .arg(harmonicListText(config.selectedHarmonics))
                                        .arg(config.refreshRate)
                                        .arg(config.signalLock));
                } else {
                    Logger::info(QStringLiteral("Device readback matches posted config."));
                }
                m_waitingConfigReadback = false;
            }
        } else {
            Logger::error(QStringLiteral("config.cgi parse failed: %1").arg(parseError));
        }
        break;
    }
    case OctivClient::RequestKind::PostConfig:
        Logger::info(QStringLiteral("config.cgi accepted by device. Reading back device config."));
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
                        //接口名，http状态码，body前512字符
}

void MainWindow::handleConnectionStateChanged(bool connected)
{
    if (isChinese()) {
        statusBar()->showMessage(connected ? QStringLiteral("已连接") : QStringLiteral("已断开"));
    } else {
        statusBar()->showMessage(connected ? QStringLiteral("Connected") : QStringLiteral("Disconnected"));
    }
}

void MainWindow::appendLog(const QString &message)
{
    ui->logTextEdit->append(message);
}

void MainWindow::updateDeviceInfo(const Octiv::DeviceInfo &info)
{
    const QString deviceName = info.serialNumber.trimmed().isEmpty()
        ? info.sensorType.trimmed()
        : info.serialNumber.trimmed();
    if (!deviceName.isEmpty()) {
        m_currentDeviceName = deviceName;
    }
    updateHarmonicOptions(info.harmonics);

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

    setSelectedHarmonics(config.selectedHarmonics);

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
        ui->channelTable->item(row, 0)->setText(QString::number(channel.timestamp));
        ui->channelTable->item(row, 1)->setText(numberText(channel.frequency, 3));
        ui->channelTable->item(row, 2)->setText(numberText(channel.voltage, 3));
        ui->channelTable->item(row, 3)->setText(numberText(channel.current, 3));
        ui->channelTable->item(row, 4)->setText(numberText(channel.phase, 3));
    }

    appendChannelsToOutput(channels);
}

bool MainWindow::beginDataRecording()//文件写入逻辑
{
    if (m_outputFile.isOpen()) {
        m_outputFile.close();
    }

    QString basePath = QCoreApplication::applicationDirPath();
#if defined(QT_DEBUG) && defined(OCTIV_SOURCE_DIR)//？
    basePath = QStringLiteral(OCTIV_SOURCE_DIR);
#endif

    QDir appDir(basePath);
    if (!appDir.exists(QStringLiteral("OutData")) && !appDir.mkpath(QStringLiteral("OutData"))) {
        Logger::error(QStringLiteral("Failed to create OutData folder under %1.").arg(appDir.absolutePath()));
        return false;
    }

    QDir outputDir(appDir.filePath(QStringLiteral("OutData")));
    QString deviceName = m_currentDeviceName.trimmed();
    if ((deviceName.isEmpty() || deviceName == QLatin1String("OctivSensor")) && !m_client->host().trimmed().isEmpty()) {
        deviceName = QStringLiteral("OctivSensor_%1").arg(m_client->host().trimmed());
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    m_outputFilePath = outputDir.filePath(QStringLiteral("%1_%2.txt").arg(safeFileNamePart(deviceName), timestamp));
    m_outputFile.setFileName(m_outputFilePath);

    if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error(QStringLiteral("Failed to open output file %1: %2").arg(m_outputFilePath, m_outputFile.errorString()));
        m_outputFilePath.clear();
        m_recordingActive = false;
        return false;
    }

    QTextStream stream(&m_outputFile);
    stream << "Timestamp\tChannel\tFrequency\tVoltage\tCurrent\tPhase\n";
    stream.flush();
    m_recordingActive = true;
    Logger::info(QStringLiteral("Realtime data recording started: %1").arg(m_outputFilePath));
    return true;
}

void MainWindow::endDataRecording()//停止采集或关闭窗口时执行
{
    if (!m_recordingActive && !m_outputFile.isOpen()) {
        return;
    }

    if (m_outputFile.isOpen()) {
        QTextStream stream(&m_outputFile);
        stream.flush();
        m_outputFile.close();
    }

    m_recordingActive = false;
    if (!m_outputFilePath.isEmpty()) {
        Logger::info(QStringLiteral("Realtime data saved: %1").arg(m_outputFilePath));
    }
}

void MainWindow::appendChannelsToOutput(const QVector<Octiv::ChannelData> &channels)
{
    if (!m_recordingActive || !m_outputFile.isOpen()) {
        return;
    }

    QTextStream stream(&m_outputFile);
    const int count = qMin(ui->channelTable->rowCount(), channels.size());
    for (int i = 0; i < count; ++i) {
        const Octiv::ChannelData &channel = channels.at(i);
        stream << channel.timestamp << '\t'
               << (channel.channel > 0 ? channel.channel : i + 1) << '\t'
               << numberText(channel.frequency, 6) << '\t'
               << numberText(channel.voltage, 6) << '\t'
               << numberText(channel.current, 6) << '\t'
               << numberText(channel.phase, 6) << '\n';
    }
    stream.flush();
}

void MainWindow::updateHarmonicOptions(const QVector<int> &harmonics)
{
    QVector<int> options;
    options.reserve(qMax(16, harmonics.size() + 1));
    options.append(0);

    if (harmonics.isEmpty()) {
        for (int value = 1; value <= 15; ++value) {
            options.append(value);
        }
    } else {
        for (const int harmonic : harmonics) {
            if (!options.contains(harmonic)) {
                options.append(harmonic);
            }
        }
    }

    std::sort(options.begin(), options.end());

    QVector<int> currentValues = selectedHarmonicsFromUi();
    while (currentValues.size() < 5) {
        currentValues.append(0);
    }

    for (QComboBox *comboBox : std::as_const(m_harmonicComboBoxes)) {
        comboBox->clear();
        for (const int option : options) {
            comboBox->addItem(QString::number(option), option);
        }
    }

    setSelectedHarmonics(currentValues);
}

void MainWindow::setSelectedHarmonics(const QVector<int> &harmonics)
{
    for (int i = 0; i < m_harmonicComboBoxes.size(); ++i) {
        QComboBox *comboBox = m_harmonicComboBoxes.at(i);
        const int value = i < harmonics.size() ? harmonics.at(i) : 0;
        int index = comboBox->findData(value);
        if (index < 0) {
            comboBox->addItem(QString::number(value), value);
            index = comboBox->findData(value);
        }
        comboBox->setCurrentIndex(index);
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

QVector<int> MainWindow::selectedHarmonicsFromUi() const//谐波字符串转换读取
{
    QVector<int> harmonics;
    harmonics.reserve(m_harmonicComboBoxes.size());
    for (const QComboBox *comboBox : m_harmonicComboBoxes) {
        harmonics.append(comboBox->currentData().toInt());
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

QString MainWindow::safeFileNamePart(const QString &text) const//名称合法检测替换为"_"
{
    QString safe = text.trimmed();
    safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\s]+")), QStringLiteral("_"));
    safe.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    safe.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    if (safe.isEmpty()) {
        return QStringLiteral("OctivSensor");
    }
    return safe.left(80);
}

QString MainWindow::harmonicListText(const QVector<int> &harmonics) const
{
    QStringList parts;
    parts.reserve(harmonics.size());
    for (const int harmonic : harmonics) {
        parts.append(QString::number(harmonic));
    }
    return parts.join(QStringLiteral(","));
}

QString MainWindow::startDataText() const
{
    return isChinese() ? QStringLiteral("开始数据") : QStringLiteral("Start Data");
}

QString MainWindow::stopDataText() const
{
    return isChinese() ? QStringLiteral("停止数据") : QStringLiteral("Stop Data");
}

bool MainWindow::isChinese() const
{
    return m_language == Language::Chinese;
}
