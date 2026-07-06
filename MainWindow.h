#pragma once

#include "model/OctivData.h"
#include "network/OctivClient.h"

#include <QMainWindow>
#include <QTimer>

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
    void setupTable();
    void updateDeviceInfo(const Octiv::DeviceInfo &info);
    void updateConfig(const Octiv::DeviceConfig &config);
    void updateTemperature(const Octiv::Temperature &temperature);
    void updateChannels(const QVector<Octiv::ChannelData> &channels);
    Octiv::DeviceConfig configFromUi() const;
    QVector<int> selectedHarmonicsFromUi() const;
    QString requestKindName(OctivClient::RequestKind kind) const;
    QString numberText(double value, int precision = 3) const;

    Ui::MainWindow *ui = nullptr;
    OctivClient *m_client = nullptr;
    QTimer *m_dataTimer = nullptr;
    int m_temperaturePollCounter = 0;
};
