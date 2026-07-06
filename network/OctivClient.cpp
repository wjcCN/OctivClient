#include "network/OctivClient.h"

#include "model/OctivData.h"
#include "utils/Logger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

OctivClient::OctivClient(QObject *parent)
    : QObject(parent),
      m_network(new QNetworkAccessManager(this))
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!m_connectRequested || m_connected) {
            return;
        }

        Logger::warning(QStringLiteral("Reconnect probe started."));
        sendGet(QStringLiteral("/octiv_service/info.cgi"), RequestKind::ReconnectProbe);
    });
}

void OctivClient::setHost(const QString &host)
{
    m_host = host.trimmed();
    if (m_host.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)) {
        m_host.remove(0, 7);
    } else if (m_host.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        m_host.remove(0, 8);
    }
    m_host = m_host.section(QLatin1Char('/'), 0, 0);
}

QString OctivClient::host() const
{
    return m_host;
}

void OctivClient::setTimeoutMs(int timeoutMs)
{
    m_timeoutMs = qMax(1000, timeoutMs);
}

int OctivClient::timeoutMs() const
{
    return m_timeoutMs;
}

void OctivClient::setAutoReconnectEnabled(bool enabled)
{
    m_autoReconnectEnabled = enabled;
}

bool OctivClient::autoReconnectEnabled() const
{
    return m_autoReconnectEnabled;
}

void OctivClient::connectToDevice()
{
    m_connectRequested = true;
    m_reconnectAttempts = 0;
    getInfo();
}

void OctivClient::getInfo()
{
    sendGet(QStringLiteral("/octiv_service/info.cgi"), RequestKind::Info);
}

void OctivClient::getConfig()
{
    sendGet(QStringLiteral("/octiv_service/config.cgi"), RequestKind::GetConfig);
}

void OctivClient::postConfig(const Octiv::DeviceConfig &config)
{
    QJsonArray harmonics;
    for (const int harmonic : config.selectedHarmonics) {
        harmonics.append(harmonic);
    }

    QJsonObject body;
    body.insert(QStringLiteral("selected_harmonics"), harmonics);
    body.insert(QStringLiteral("refresh_rate"), config.refreshRate);
    body.insert(QStringLiteral("signal_lock"), QString(config.signalLock));

    sendPost(QStringLiteral("/octiv_service/config.cgi"), body, RequestKind::PostConfig);
}

void OctivClient::getTemperature()
{
    sendGet(QStringLiteral("/octiv_service/temperature.cgi"), RequestKind::Temperature);
}

void OctivClient::getData()
{
    sendGet(QStringLiteral("/octiv_service/data.cgi"), RequestKind::Data);
}

void OctivClient::getIonFluxParams()
{
    sendGet(QStringLiteral("/octiv_service/ionfluxparams.cgi"), RequestKind::GetIonFluxParams);
}

void OctivClient::postIonFluxParams(const Octiv::IonFluxParams &params)
{
    QJsonObject body;
    body.insert(QStringLiteral("voltage_drop"), params.voltageDrop);
    body.insert(QStringLiteral("series_resistance"), params.seriesResistance);
    body.insert(QStringLiteral("electrode_area"), params.electrodeArea);

    sendPost(QStringLiteral("/octiv_service/ionfluxparams.cgi"), body, RequestKind::PostIonFluxParams);
}

QUrl OctivClient::endpoint(const QString &path) const
{
    return QUrl(QStringLiteral("http://%1%2").arg(m_host, path));
}

void OctivClient::sendGet(const QString &path, RequestKind kind)
{
    QNetworkRequest request(endpoint(path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(m_timeoutMs);
    sendRequest(request, QByteArray(), QStringLiteral("GET"), kind);
}

void OctivClient::sendPost(const QString &path, const QJsonObject &body, RequestKind kind)
{
    QNetworkRequest request(endpoint(path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(m_timeoutMs);

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    sendRequest(request, payload, QStringLiteral("POST"), kind);
}

void OctivClient::sendRequest(const QNetworkRequest &request, const QByteArray &body, const QString &method, RequestKind kind)
{
    QNetworkReply *reply = nullptr;
    if (method == QLatin1String("GET")) {
        reply = m_network->get(request);
    } else if (method == QLatin1String("POST")) {
        reply = m_network->post(request, body);
    }

    if (!reply) {
        Logger::error(QStringLiteral("Unsupported HTTP method: %1").arg(method));
        emit requestFailed(kind, 405, httpStatusText(405), QByteArray());
        return;
    }

    Logger::http(QStringLiteral("%1 %2").arg(method, request.url().toString()));
    emit requestStarted(kind, method, request.url());

    connect(reply, &QNetworkReply::finished, this, [this, reply, kind]() {
        handleFinished(reply, kind);
    });
}

void OctivClient::handleFinished(QNetworkReply *reply, RequestKind kind)
{
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const bool transportOk = reply->error() == QNetworkReply::NoError;
    const bool httpOk = httpStatus == 200;

    if (transportOk && httpOk) {
        Logger::http(QStringLiteral("200 OK %1").arg(reply->url().path()));
        setConnected(true);
        m_reconnectAttempts = 0;
        emit rawResponse(kind, payload, httpStatus);
    } else {
        const int status = httpStatus == 0 ? -1 : httpStatus;
        const QString statusText = httpStatus == 0 ? reply->errorString() : httpStatusText(httpStatus);
        Logger::error(QStringLiteral("%1 %2 %3").arg(status).arg(statusText, reply->url().path()));
        setConnected(false);
        emit requestFailed(kind, status, statusText, payload);
        scheduleReconnect();
    }

    reply->deleteLater();
}

void OctivClient::scheduleReconnect()
{
    if (!m_autoReconnectEnabled || !m_connectRequested || m_reconnectTimer.isActive()) {
        return;
    }

    ++m_reconnectAttempts;
    const int delayMs = qMin(10000, 1000 * m_reconnectAttempts);
    Logger::warning(QStringLiteral("Auto reconnect scheduled in %1 ms.").arg(delayMs));
    m_reconnectTimer.start(delayMs);
}

void OctivClient::setConnected(bool connected)
{
    if (m_connected == connected) {
        return;
    }

    m_connected = connected;
    emit connectionStateChanged(m_connected);
}

QString OctivClient::httpStatusText(int statusCode)
{
    switch (statusCode) {
    case 200:
        return QStringLiteral("OK");
    case 400:
        return QStringLiteral("BadRequest - invalid parameters");
    case 401:
        return QStringLiteral("Unauthorized - token expired");
    case 404:
        return QStringLiteral("NotFound - URL error");
    case 405:
        return QStringLiteral("MethodNotAllowed");
    case 429:
        return QStringLiteral("RateLimitExceeded");
    case 500:
        return QStringLiteral("InternalError");
    default:
        return QStringLiteral("HTTP %1").arg(statusCode);
    }
}
