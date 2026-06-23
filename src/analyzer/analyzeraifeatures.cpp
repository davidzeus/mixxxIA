#include "analyzer/analyzeraifeatures.h"

#include <QDateTime>

#include "ai/aisettings.h"
#include "ai/aisidecarclient.h"
#include "analyzer/analyzertrack.h"
#include "track/track.h"
#include "util/logger.h"

namespace {
mixxx::Logger kLogger("AnalyzerAiFeatures");
} // namespace

AnalyzerAiFeatures::AnalyzerAiFeatures(
        UserSettingsPointer pConfig, const QSqlDatabase& dbConnection)
        : m_pConfig(std::move(pConfig)),
          m_active(false) {
    m_dao.initialize(dbConnection);
}

bool AnalyzerAiFeatures::initialize(const AnalyzerTrack& track,
        mixxx::audio::SampleRate sampleRate,
        SINT frameLength) {
    Q_UNUSED(sampleRate);
    Q_UNUSED(frameLength);

    if (!mixxx::ai::isAnalysisEnabled(m_pConfig)) {
        kLogger.info() << "[IA] Analisis IA desactivado; se omite el embedding";
        return false;
    }
    const TrackPointer pTrack = track.getTrack();
    if (!pTrack) {
        return false;
    }
    m_trackId = pTrack->getId();
    m_filePath = pTrack->getLocation();
    if (m_filePath.isEmpty()) {
        return false;
    }
    // Skip tracks that already have AI features to avoid redundant work.
    if (m_trackId.isValid() && m_dao.hasFeatures(m_trackId)) {
        kLogger.info() << "[IA] El tema ya tiene features IA; se omite:" << m_filePath;
        return false;
    }
    kLogger.info() << "[IA] Programado para analisis IA:" << m_filePath;
    m_active = true;
    return true;
}

bool AnalyzerAiFeatures::processSamples(const CSAMPLE* pIn, SINT count) {
    Q_UNUSED(pIn);
    Q_UNUSED(count);
    // The sidecar reads the audio file itself; no need to buffer PCM here.
    return true;
}

void AnalyzerAiFeatures::storeResults(TrackPointer pTrack) {
    if (!m_active || !pTrack) {
        return;
    }
    const TrackId trackId = pTrack->getId();
    if (!trackId.isValid()) {
        kLogger.debug() << "Skipping AI features: track has no id yet"
                        << m_filePath;
        return;
    }

    const QString url = mixxx::ai::sidecarUrl(m_pConfig);
    kLogger.info() << "[IA] Enviando al sidecar" << url << "tema:" << m_filePath;
    AiSidecarClient client(url);
    TrackAiFeatures features;
    QString error;
    if (!client.embedFile(m_filePath, &features, &error)) {
        kLogger.warning() << "[IA] Fallo del sidecar para" << m_filePath << ":" << error;
        return;
    }

    features.trackId = trackId;
    features.analyzedAt = QDateTime::currentSecsSinceEpoch();
    if (m_dao.saveFeatures(features)) {
        kLogger.info() << "[IA] Features IA guardadas para" << m_filePath
                       << "dim=" << features.embedding.size()
                       << "genero=" << features.genre;
    } else {
        kLogger.warning() << "[IA] No se pudieron guardar las features IA para" << m_filePath;
    }
}

void AnalyzerAiFeatures::cleanup() {
    m_active = false;
    m_filePath.clear();
    m_trackId = TrackId();
}
