#pragma once

#include <QString>

#include "ai/aisidecarclient.h"
#include "preferences/usersettings.h"

/// Centralized accessors for the "[AI]" preferences group so the analyzer,
/// the Automix selector and the preferences page all agree on key names and
/// defaults.
namespace mixxx {
namespace ai {

inline const char* kConfigGroup = "[AI]";

/// Master switch: enables both AI analysis and AI Automix.
inline bool isEnabled(const UserSettingsPointer& pConfig) {
    return pConfig->getValue<bool>(
            ConfigKey(kConfigGroup, QStringLiteral("Enabled")), false);
}

/// Whether the analyzer should compute embeddings while scanning tracks.
inline bool isAnalysisEnabled(const UserSettingsPointer& pConfig) {
    return isEnabled(pConfig) &&
            pConfig->getValue<bool>(
                    ConfigKey(kConfigGroup, QStringLiteral("AnalyzeEnabled")),
                    true);
}

/// Whether AI should drive next-track selection in AutoDJ.
inline bool isAutomixEnabled(const UserSettingsPointer& pConfig) {
    return isEnabled(pConfig) &&
            pConfig->getValue<bool>(
                    ConfigKey(kConfigGroup, QStringLiteral("AutomixEnabled")),
                    true);
}

inline QString sidecarUrl(const UserSettingsPointer& pConfig) {
    const QString url = pConfig->getValueString(
            ConfigKey(kConfigGroup, QStringLiteral("SidecarUrl")));
    return url.isEmpty() ? AiSidecarClient::kDefaultBaseUrl : url;
}

/// Scorer weights for the Automix selector. Defaults sum to 1.0.
struct ScorerWeights {
    double embedding = 0.5;
    double key = 0.2;
    double bpm = 0.2;
    double energy = 0.1;
};

inline ScorerWeights scorerWeights(const UserSettingsPointer& pConfig) {
    ScorerWeights w;
    const auto read = [&](const char* key, double fallback) {
        const QString s = pConfig->getValueString(ConfigKey(kConfigGroup, key));
        bool ok = false;
        const double v = s.toDouble(&ok);
        return ok ? v : fallback;
    };
    w.embedding = read("WeightEmbedding", w.embedding);
    w.key = read("WeightKey", w.key);
    w.bpm = read("WeightBpm", w.bpm);
    w.energy = read("WeightEnergy", w.energy);
    return w;
}

/// Max BPM difference (as a fraction, e.g. 0.08 = 8%) for a track to be a
/// valid Automix candidate.
inline double maxBpmFraction(const UserSettingsPointer& pConfig) {
    const QString s = pConfig->getValueString(
            ConfigKey(kConfigGroup, QStringLiteral("MaxBpmFraction")));
    bool ok = false;
    const double v = s.toDouble(&ok);
    return (ok && v > 0.0) ? v : 0.08;
}

} // namespace ai
} // namespace mixxx
