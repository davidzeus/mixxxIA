#pragma once

#include <QString>
#include <QStringList>

#include "library/dao/aifeaturedao.h"

/// Minimal blocking HTTP client for the Mixxx AI sidecar (see
/// tools/ai_sidecar/). Designed to be called from a worker thread: each call
/// spins a local QEventLoop, so it does not require the calling thread to run
/// its own event loop.
class AiSidecarClient {
  public:
    /// Default base URL of the local sidecar service.
    static const QString kDefaultBaseUrl;

    explicit AiSidecarClient(QString baseUrl = kDefaultBaseUrl,
            int timeoutMs = 120000);

    /// Ask the sidecar to analyze a file and return its embedding + tags.
    /// Blocks until the request completes, fails, or times out.
    /// On success fills *pFeatures (without trackId/analyzedAt, which the
    /// caller sets) and returns true. On failure returns false and, if
    /// pError is non-null, sets a human-readable message.
    bool embedFile(const QString& filePath,
            TrackAiFeatures* pFeatures,
            QString* pError = nullptr) const;

    /// Returns true if the sidecar answers /health. Optionally reports the
    /// active backend (e.g. "clap-htsat-unfused-v1").
    bool checkHealth(QString* pBackend = nullptr) const;

    /// Resolve a free-text request (e.g. "switch to reggaeton") to one of the
    /// available genres via the configured LLM provider. On success sets
    /// *pTargetGenre (may be empty if the LLM couldn't map it) and returns
    /// true. The result is always one of availableGenres or empty.
    bool resolveGenre(const QString& request,
            const QString& currentGenre,
            const QStringList& availableGenres,
            const QString& provider,
            const QString& apiKey,
            const QString& model,
            QString* pTargetGenre,
            QString* pError = nullptr) const;

  private:
    QString m_baseUrl;
    int m_timeoutMs;
};
