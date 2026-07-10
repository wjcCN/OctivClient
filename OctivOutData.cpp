#include "OctivOutData.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTextStream>

#include <cmath>

OctivOutData::OctivOutData(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    initUi();
    connect(ui.startButton, &QPushButton::clicked, this, &OctivOutData::start);
    connect(ui.stopButton, &QPushButton::clicked, this, &OctivOutData::stop);
    connect(ui.frequencyBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        QSignalBlocker blocker(ui.harmonicBox);
        ui.harmonicBox->setCurrentIndex(m_harmonics.at(index));
    });
    connect(ui.harmonicBox, &QComboBox::currentIndexChanged, this, [this](int harmonic) {
        m_harmonics[ui.frequencyBox->currentIndex()] = harmonic;
        pushHarmonic();
    });
    connect(ui.samplingBox, &QComboBox::currentIndexChanged, this, &OctivOutData::switchSamplingRate);
    connect(&m_timer, &QTimer::timeout, this, &OctivOutData::readData);
}

OctivOutData::~OctivOutData()
{
    stop();
}

void OctivOutData::initUi()
{
    ui.frequencyBox->addItem(QStringLiteral("400kHz"), 400000.0);
    ui.frequencyBox->addItem(QStringLiteral("2MHz"), 2000000.0);
    ui.frequencyBox->addItem(QStringLiteral("13.56MHz"), 13560000.0);
    ui.frequencyBox->addItem(QStringLiteral("27.12MHz"), 27120000.0);
    ui.frequencyBox->addItem(QStringLiteral("60MHz"), 60000000.0);
    m_harmonics = QVector<int>(ui.frequencyBox->count(), 0);

    for (int i = 0; i <= 10; ++i) {
        ui.harmonicBox->addItem(QString::number(i), i);
    }

    ui.samplingBox->addItem(QStringLiteral("100"), 100);
    ui.samplingBox->addItem(QStringLiteral("200"), 200);
    ui.samplingBox->addItem(QStringLiteral("500"), 500);
    ui.samplingBox->addItem(QStringLiteral("1000"), 1000);

    ui.stopButton->setEnabled(false);
    ui.dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int column = 0; column < ui.dataTable->columnCount(); ++column) {
        ui.dataTable->setItem(0, column, new QTableWidgetItem(QStringLiteral("--")));
    }
}

void OctivOutData::start()
{
    QDir dir(outDirPath());
    dir.mkpath(QStringLiteral("."));

    if (m_file.isOpen()) {
        m_file.close();
    }

    const QString name = QStringLiteral("%1_%2.txt")
        .arg(ui.frequencyBox->currentText())
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_file.setFileName(dir.filePath(name));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui.statusBar->showMessage(tr("无法创建输出文件：%1").arg(m_file.fileName()));
        return;
    }

    QTextStream stream(&m_file);
    stream << "Frequency\tHarmonic\tTimestamp\tVoltage\tCurrent\n";

    pushHarmonic();
    readData();
    m_timer.start(100);
    ui.startButton->setEnabled(false);
    ui.stopButton->setEnabled(true);
    ui.statusBar->showMessage(tr("正在采集：%1").arg(m_file.fileName()));
}

void OctivOutData::stop()
{
    m_timer.stop();
    if (m_file.isOpen()) {
        QTextStream stream(&m_file);
        stream.flush();
        m_file.close();
    }
    ui.startButton->setEnabled(true);
    ui.stopButton->setEnabled(false);
}

void OctivOutData::pushHarmonic()
{
    QJsonArray selectedHarmonics;
    for (const int harmonic : m_harmonics) {
        selectedHarmonics.append(harmonic);
    }

    QJsonObject body;
    body.insert(QStringLiteral("selected_harmonics"), selectedHarmonics);
    body.insert(QStringLiteral("refresh_rate"), selectedSamplingRate());
    body.insert(QStringLiteral("signal_lock"), QStringLiteral("V"));

    QNetworkRequest request(QUrl(QStringLiteral("http://%1/octiv_service/config.cgi").arg(ui.ipEdit->text().trimmed())));//.trimmed作用：去除首尾空白字符
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void OctivOutData::switchSamplingRate()
{
    pushHarmonic();
}

void OctivOutData::readData()
{
    const QUrl url(QStringLiteral("http://%1/octiv_service/data.cgi").arg(ui.ipEdit->text().trimmed()));
    QNetworkReply *reply = m_network.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleData(reply->readAll());
        reply->deleteLater();
    });
}

void OctivOutData::handleData(const QByteArray &payload)
{
    QJsonDocument doc = QJsonDocument::fromJson(payload);
    QJsonArray array = doc.isArray() ? doc.array() : doc.object().value(QStringLiteral("data")).toArray();
    const double wanted = selectedFrequency();
    const int harmonic = ui.harmonicBox->currentData().toInt();

    QJsonObject selected;
    for (const QJsonValue &value : array) {
        QJsonObject obj = value.toObject();
        if (obj.value(QStringLiteral("frequency")).toDouble() == wanted) {
            selected = obj;
            break;
        }
    }

    if (selected.isEmpty() && ui.frequencyBox->currentIndex() < array.size()) {
        selected = array.at(ui.frequencyBox->currentIndex()).toObject();
    }

    const QString timestamp = arrayText(selected.value(QStringLiteral("timestamp")), 0);
    const QString voltage = arrayText(selected.value(QStringLiteral("voltage")), harmonic);
    const QString current = arrayText(selected.value(QStringLiteral("current")), harmonic);

    ui.dataTable->item(0, 0)->setText(ui.frequencyBox->currentText());
    ui.dataTable->item(0, 1)->setText(QString::number(harmonic));
    ui.dataTable->item(0, 2)->setText(timestamp);
    ui.dataTable->item(0, 3)->setText(voltage);
    ui.dataTable->item(0, 4)->setText(current);

    if (m_file.isOpen()) {
        QTextStream stream(&m_file);
        stream << ui.frequencyBox->currentText() << '\t'
               << harmonic << '\t'
               << timestamp << '\t'
               << voltage << '\t'
               << current << '\n';
        stream.flush();
    }
}

QString OctivOutData::outDirPath() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/OutData");
}

double OctivOutData::selectedFrequency() const
{
    return ui.frequencyBox->currentData().toDouble();
}

int OctivOutData::selectedSamplingRate() const
{
    return ui.samplingBox->currentData().toInt();
}

QString OctivOutData::jsonText(const QJsonValue &value) const
{
    if (value.isString()) {
        return value.toString();
    }

    const double number = value.toDouble();
    if (std::floor(number) == number) {
        return QString::number(static_cast<qint64>(number));
    }
    return QString::number(number, 'f', 6);
}

QString OctivOutData::arrayText(const QJsonValue &value, int index) const
{
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        if (index >= 0 && index < array.size()) {
            return jsonText(array.at(index));
        }
        return QStringLiteral("--");
    }
    return jsonText(value);
}
