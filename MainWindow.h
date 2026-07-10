#pragma once

#include "model/OctivData.h"
#include "network/OctivClient.h"

#include <QFile>
#include <QMainWindow>
#include <QTimer>

class QComboBox;
class QLabel;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onGetInfoClicked();
    void onStartStopClicked();
    void onGetConfigClicked();
    void onApplyConfigClicked();
    void pollRealtimeData();
    void handleRawResponse(OctivClient::RequestKind kind, const QByteArray &payload, int httpStatus);
    void handleRequestFailed(OctivClient::RequestKind kind, int httpStatus, const QString &statusText, const QByteArray &payload);
    void handleConnectionStateChanged(bool connected);
    void appendLog(const QString &message);

private:
    enum class Language {
        Chinese,
        English
    };

    void setupLanguageControls();
    void setupHarmonicControls();
    void applyLanguage();
    void setupTable();
    void updateDeviceInfo(const Octiv::DeviceInfo &info);
    void updateConfig(const Octiv::DeviceConfig &config);
    void updateTemperature(const Octiv::Temperature &temperature);
    void updateChannels(const QVector<Octiv::ChannelData> &channels);
    bool beginDataRecording();
    void endDataRecording();
    void appendChannelsToOutput(const QVector<Octiv::ChannelData> &channels);
    void updateHarmonicOptions(const QVector<int> &harmonics);
    void setSelectedHarmonics(const QVector<int> &harmonics);
    Octiv::DeviceConfig configFromUi() const;
    QVector<int> selectedHarmonicsFromUi() const;
    QString requestKindName(OctivClient::RequestKind kind) const;
    QString numberText(double value, int precision = 3) const;
    QString safeFileNamePart(const QString &text) const;
    QString harmonicListText(const QVector<int> &harmonics) const;
    QString startDataText() const;
    QString stopDataText() const;
    bool isChinese() const;

    Ui::MainWindow *ui = nullptr;
    OctivClient *m_client = nullptr;
    QTimer *m_dataTimer = nullptr;
    QLabel *m_languageLabel = nullptr;
    QComboBox *m_languageComboBox = nullptr;
    QWidget *m_harmonicsWidget = nullptr;
    QVector<QLabel *> m_harmonicLabels;
    QVector<QComboBox *> m_harmonicComboBoxes;
    Language m_language = Language::Chinese;
    QFile m_outputFile;
    QString m_outputFilePath;
    QString m_currentDeviceName = QStringLiteral("OctivSensor");
    bool m_recordingActive = false;
    bool m_waitingConfigReadback = false;
    Octiv::DeviceConfig m_lastPostedConfig;
    int m_temperaturePollCounter = 0;
};
