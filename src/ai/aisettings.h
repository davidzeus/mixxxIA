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

// --- Natural-language layer (Phase 5) -----------------------------------
// The LLM is only used to interpret free-text requests like "switch to
// reggaeton". It can run locally (Ollama, model auto-pulled) or via a cloud
// provider (Google Gemini / OpenAI / Anthropic) with an API key.

/// "local" | "google" | "openai" | "anthropic". Default "local".
inline QString llmProvider(const UserSettingsPointer& pConfig) {
    const QString p = pConfig->getValueString(
            ConfigKey(kConfigGroup, QStringLiteral("LlmProvider")));
    return p.isEmpty() ? QStringLiteral("local") : p;
}

/// API key for the chosen cloud provider (unused for "local").
inline QString llmApiKey(const UserSettingsPointer& pConfig) {
    return pConfig->getValueString(
            ConfigKey(kConfigGroup, QStringLiteral("LlmApiKey")));
}

/// Optional explicit model id; empty means "let the sidecar pick a sensible
/// default for the provider" (and auto-download it for local).
inline QString llmModel(const UserSettingsPointer& pConfig) {
    return pConfig->getValueString(
            ConfigKey(kConfigGroup, QStringLiteral("LlmModel")));
}

/// Whether the natural-language request box is usable (a provider is local,
/// or a cloud provider has an API key configured).
inline bool isNaturalLanguageEnabled(const UserSettingsPointer& pConfig) {
    if (!isEnabled(pConfig)) {
        return false;
    }
    return llmProvider(pConfig) == QStringLiteral("local") ||
            !llmApiKey(pConfig).isEmpty();
}

} // namespace ai
} // namespace mixxx
