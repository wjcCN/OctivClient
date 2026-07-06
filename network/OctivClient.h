#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QJsonObject;

namespace Octiv {
struct DeviceConfig;
struct IonFluxParams;
}

class OctivClient : public QObject
{
    Q_OBJECT

public:
    enum class RequestKind {
        Info,
        GetConfig,
        PostConfig,
        Temperature,
        Data,
        GetIonFluxParams,
        PostIonFluxParams,
        ReconnectProbe
    };
    Q_ENUM(RequestKind)

    explicit OctivClient(QObject *parent = nullptr);

    void setHost(const QString &host);
    QString host() const;

    void setTimeoutMs(int timeoutMs);
    int timeoutMs() const;

    void setAutoReconnectEnabled(bool enabled);
    bool autoReconnectEnabled() const;

    void connectToDevice();
    void getInfo();
    void getConfig();
    void postConfig(const Octiv::DeviceConfig &config);
    void getTemperature();
    void getData();
    void getIonFluxParams();
    void postIonFluxParams(const Octiv::IonFluxParams &params);

signals:
    void requestStarted(OctivClient::RequestKind kind, const QString &method, const QUrl &url);
    void rawResponse(OctivClient::RequestKind kind, const QByteArray &payload, int httpStatus);
    void requestFailed(OctivClient::RequestKind kind, int httpStatus, const QString &statusText, const QByteArray &payload);
    void connectionStateChanged(bool connected);

private:
    QUrl endpoint(const QString &path) const;
    void sendGet(const QString &path, RequestKind kind);
    void sendPost(const QString &path, const QJsonObject &body, RequestKind kind);
    void sendRequest(const QNetworkRequest &request, const QByteArray &body, const QString &method, RequestKind kind);
    void handleFinished(QNetworkReply *reply, RequestKind kind);
    void scheduleReconnect();
    void setConnected(bool connected);

    static QString httpStatusText(int statusCode);

    QNetworkAccessManager *m_network = nullptr;
    QString m_host = QStringLiteral("192.168.18.52");
    int m_timeoutMs = 5000;
    bool m_autoReconnectEnabled = true;
    bool m_connectRequested = false;
    bool m_connected = false;
    int m_reconnectAttempts = 0;
    QTimer m_reconnectTimer;
};
