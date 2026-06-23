#pragma once

#include <QSqlDatabase>
#include <QString>

#include "analyzer/analyzer.h"
#include "library/dao/aifeaturedao.h"
#include "preferences/usersettings.h"
#include "track/trackid.h"

class AnalyzerTrack;

/// Analyzer that computes a per-track audio embedding + tags by delegating to
/// the local AI sidecar, and stores the result in track_ai_features.
///
/// Unlike the DSP analyzers it does not consume PCM in processSamples(): the
/// sidecar reads the audio file directly (it needs full-resolution audio at
/// its own sample rate). This analyzer therefore only needs the track's file
/// location, captured in initialize(), and does its network + DB work in
/// storeResults().
class AnalyzerAiFeatures : public Analyzer {
  public:
    AnalyzerAiFeatures(UserSettingsPointer pConfig, const QSqlDatabase& dbConnection);
    ~AnalyzerAiFeatures() override = default;

    bool initialize(const AnalyzerTrack& track,
            mixxx::audio::SampleRate sampleRate,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* pIn, SINT count) override;
    void storeResults(TrackPointer pTrack) override;
    void cleanup() override;

  private:
    UserSettingsPointer m_pConfig;
    AiFeatureDao m_dao;
    bool m_active;
    QString m_filePath;
    TrackId m_trackId;
};
