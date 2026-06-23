#include "ai/aisidecarclient.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "util/logger.h"

namespace {
mixxx::Logger kLogger("AiSidecarClient");
} // namespace

const QString AiSidecarClient::kDefaultBaseUrl =
        QStringLiteral("http://127.0.0.1:8765");

AiSidecarClient::AiSidecarClient(QString baseUrl, int timeoutMs)
        : m_baseUrl(std::move(baseUrl)),
          m_timeoutMs(timeoutMs) {
    // Normalize: drop a trailing slash so endpoint concatenation is clean.
    while (m_baseUrl.endsWith('/')) {
        m_baseUrl.chop(1);
    }
}

namespace {

/// Perform a blocking HTTP request and return the response body.
/// Returns false (and sets *pError) on transport error or timeout.
bool blockingRequest(QNetworkAccessManager* pNam,
        const QNetworkRequest& request,
        const QByteArray& postBody, // empty -> GET
        int timeoutMs,
        QByteArray* pResponse,
        QString* pError) {
    QNetworkReply* pReply = postBody.isEmpty()
            ? pNam->get(request)
            : pNam->post(request, postBody);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    bool ok = false;
    if (!timer.isActive()) {
        // Timer fired first -> timed out.
        pReply->abort();
        if (pError) {
            *pError = QStringLiteral("AI sidecar request timed out");
        }
    } else {
        timer.stop();
        if (pReply->error() != QNetworkReply::NoError) {
            if (pError) {
                *pError = pReply->errorString();
            }
        } else {
            if (pResponse) {
                *pResponse = pReply->readAll();
            }
            ok = true;
        }
    }
    pReply->deleteLater();
    return ok;
}

} // namespace

bool AiSidecarClient::embedFile(const QString& filePath,
        TrackAiFeatures* pFeatures,
        QString* pError) const {
    if (!pFeatures) {
        return false;
    }

    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(m_baseUrl + QStringLiteral("/embed"))};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
            QStringLiteral("application/json"));

    QJsonObject body;
    body.insert(QStringLiteral("path"), filePath);
    const QByteArray postBody = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QByteArray response;
    if (!blockingRequest(&nam, request, postBody, m_timeoutMs, &response, pError)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        if (pError) {
            *pError = QStringLiteral("AI sidecar returned malformed JSON");
        }
        return false;
    }
    const QJsonObject obj = doc.object();
    const QJsonArray embeddingArray = obj.value(QStringLiteral("embedding")).toArray();
    if (embeddingArray.isEmpty()) {
        if (pError) {
            *pError = QStringLiteral("AI sidecar returned empty embedding");
        }
        return false;
    }

    QVector<float> embedding;
    embedding.reserve(embeddingArray.size());
    for (const QJsonValue& v : embeddingArray) {
        embedding.append(static_cast<float>(v.toDouble()));
    }

    pFeatures->embedding = std::move(embedding);
    pFeatures->modelVersion = obj.value(QStringLiteral("model_version")).toString();
    pFeatures->genre = obj.value(QStringLiteral("genre")).toString();
    pFeatures->genreConfidence =
            obj.value(QStringLiteral("genre_confidence")).toDouble();
    pFeatures->mood = obj.value(QStringLiteral("mood")).toString();
    pFeatures->energy = obj.value(QStringLiteral("energy")).toDouble();
    pFeatures->danceability =
            obj.value(QStringLiteral("danceability")).toDouble();
    return true;
}

bool AiSidecarClient::checkHealth(QString* pBackend) const {
    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(m_baseUrl + QStringLiteral("/health"))};

    QByteArray response;
    QString error;
    if (!blockingRequest(&nam, request, QByteArray(), m_timeoutMs, &response, &error)) {
        kLogger.debug() << "AI sidecar health check failed:" << error;
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        return false;
    }
    if (pBackend) {
        *pBackend = doc.object().value(QStringLiteral("backend")).toString();
    }
    return doc.object().value(QStringLiteral("status")).toString() ==
            QStringLiteral("ok");
}
