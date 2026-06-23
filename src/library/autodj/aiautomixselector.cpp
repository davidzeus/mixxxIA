#include "library/autodj/aiautomixselector.h"

#include <algorithm>
#include <cmath>

#include <QPair>

#include "ai/aisettings.h"
#include "library/dao/aifeaturedao.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "util/logger.h"

namespace {
mixxx::Logger kLogger("AiAutomixSelector");

double cosineSimilarity(const QVector<float>& a, const QVector<float>& b) {
    if (a.isEmpty() || a.size() != b.size()) {
        return 0.0;
    }
    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (int i = 0; i < a.size(); ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        normA += static_cast<double>(a[i]) * a[i];
        normB += static_cast<double>(b[i]) * b[i];
    }
    if (normA <= 0.0 || normB <= 0.0) {
        return 0.0;
    }
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}

/// 0..1 harmonic compatibility between two musical keys.
double keyCompatibility(mixxx::track::io::key::ChromaticKey from,
        mixxx::track::io::key::ChromaticKey to) {
    using namespace mixxx::track::io::key;
    if (from == INVALID || to == INVALID) {
        return 0.5; // unknown key -> neutral, don't penalize
    }
    if (from == to) {
        return 1.0;
    }
    const QList<ChromaticKey> compatible = KeyUtils::getCompatibleKeys(from);
    if (compatible.contains(to)) {
        return 0.9;
    }
    const int steps = std::abs(KeyUtils::shortestStepsToCompatibleKey(from, to));
    return std::max(0.0, 0.8 - 0.15 * steps);
}

} // namespace

AiAutomixSelector::AiAutomixSelector(UserSettingsPointer pConfig,
        TrackCollectionManager* pTrackCollectionManager)
        : m_pConfig(std::move(pConfig)),
          m_pTrackCollectionManager(pTrackCollectionManager) {
}

void AiAutomixSelector::setTargetGenre(const QString& genre) {
    m_targetGenre = genre.trimmed();
}

QString AiAutomixSelector::storedGenre(const TrackPointer& pTrack) const {
    if (!pTrack || !pTrack->getId().isValid() || !m_pTrackCollectionManager) {
        return QString();
    }
    TrackCollection* pCollection = m_pTrackCollectionManager->internalCollection();
    if (!pCollection) {
        return QString();
    }
    TrackAiFeatures features;
    if (pCollection->getAiFeatureDAO().getFeatures(pTrack->getId(), &features)) {
        return features.genre;
    }
    return QString();
}

TransitionScoreBreakdown AiAutomixSelector::computeScores(
        const TrackPointer& pFrom, const TrackPointer& pTo) const {
    TransitionScoreBreakdown bd;
    if (!pFrom || !pTo) {
        bd.rejected = true;
        return bd;
    }

    // --- BPM (hard filter + score) ---
    const double bpmFrom = pFrom->getBpm();
    const double bpmTo = pTo->getBpm();
    const double maxFrac = mixxx::ai::maxBpmFraction(m_pConfig);
    bd.bpmScore = 1.0;
    if (bpmFrom > 0.0 && bpmTo > 0.0) {
        const double frac = std::abs(bpmTo - bpmFrom) / bpmFrom;
        if (frac > maxFrac) {
            bd.rejected = true;
            bd.overall = -1.0;
            return bd;
        }
        bd.bpmScore = 1.0 - frac / maxFrac;
    }

    // --- Key (harmonic mixing) ---
    bd.keyScore = keyCompatibility(pFrom->getKey(), pTo->getKey());

    // --- Embedding similarity + energy continuity (from stored AI features) ---
    bd.embeddingScore = 0.5; // neutral when features are missing
    bd.energyScore = 0.5;
    TrackCollection* pCollection = m_pTrackCollectionManager
            ? m_pTrackCollectionManager->internalCollection()
            : nullptr;
    if (pCollection) {
        AiFeatureDao& dao = pCollection->getAiFeatureDAO();
        TrackAiFeatures fromFeatures;
        TrackAiFeatures toFeatures;
        const bool haveFrom = pFrom->getId().isValid() &&
                dao.getFeatures(pFrom->getId(), &fromFeatures);
        const bool haveTo = pTo->getId().isValid() &&
                dao.getFeatures(pTo->getId(), &toFeatures);
        if (haveFrom && haveTo) {
            if (fromFeatures.embedding.size() == toFeatures.embedding.size() &&
                    !fromFeatures.embedding.isEmpty()) {
                const double cosine = cosineSimilarity(
                        fromFeatures.embedding, toFeatures.embedding);
                bd.embeddingScore = (cosine + 1.0) / 2.0; // map [-1,1] -> [0,1]
            }
            bd.energyScore = 1.0 -
                    std::min(1.0, std::abs(fromFeatures.energy - toFeatures.energy));
        }
    }

    const mixxx::ai::ScorerWeights w = mixxx::ai::scorerWeights(m_pConfig);
    const double weighted = w.embedding * bd.embeddingScore +
            w.key * bd.keyScore +
            w.bpm * bd.bpmScore +
            w.energy * bd.energyScore;
    const double weightSum = w.embedding + w.key + w.bpm + w.energy;
    bd.overall = weightSum > 0.0 ? weighted / weightSum : 0.0;
    return bd;
}

double AiAutomixSelector::scoreTransition(
        const TrackPointer& pFrom, const TrackPointer& pTo) const {
    return computeScores(pFrom, pTo).overall;
}

TransitionScoreBreakdown AiAutomixSelector::scoreTransitionDetailed(
        const TrackPointer& pFrom, const TrackPointer& pTo) const {
    return computeScores(pFrom, pTo);
}

TransitionScoreBreakdown AiAutomixSelector::computeScoresFromFeatures(
        const TrackPointer& pFrom,
        const TrackAiFeatures& fromFeat,
        const TrackPointer& pTo,
        const TrackAiFeatures& toFeat) const {
    TransitionScoreBreakdown bd;
    if (!pFrom || !pTo) {
        bd.rejected = true;
        return bd;
    }

    // --- BPM (hard filter + score) ---
    const double bpmFrom = pFrom->getBpm();
    const double bpmTo = pTo->getBpm();
    const double maxFrac = mixxx::ai::maxBpmFraction(m_pConfig);
    bd.bpmScore = 1.0;
    if (bpmFrom > 0.0 && bpmTo > 0.0) {
        const double frac = std::abs(bpmTo - bpmFrom) / bpmFrom;
        if (frac > maxFrac) {
            bd.rejected = true;
            bd.overall = -1.0;
            return bd;
        }
        bd.bpmScore = 1.0 - frac / maxFrac;
    }

    // --- Key (harmonic mixing) ---
    bd.keyScore = keyCompatibility(pFrom->getKey(), pTo->getKey());

    // --- Embedding + energy from pre-fetched features ---
    bd.embeddingScore = 0.5;
    bd.energyScore = 0.5;
    if (fromFeat.isValid() && toFeat.isValid()) {
        if (fromFeat.embedding.size() == toFeat.embedding.size() &&
                !fromFeat.embedding.isEmpty()) {
            const double cosine =
                    cosineSimilarity(fromFeat.embedding, toFeat.embedding);
            bd.embeddingScore = (cosine + 1.0) / 2.0;
        }
        bd.energyScore = 1.0 -
                std::min(1.0, std::abs(fromFeat.energy - toFeat.energy));
    }

    const mixxx::ai::ScorerWeights w = mixxx::ai::scorerWeights(m_pConfig);
    const double weighted = w.embedding * bd.embeddingScore +
            w.key * bd.keyScore +
            w.bpm * bd.bpmScore +
            w.energy * bd.energyScore;
    const double weightSum = w.embedding + w.key + w.bpm + w.energy;
    bd.overall = weightSum > 0.0 ? weighted / weightSum : 0.0;
    return bd;
}

QList<TrackRecommendation> AiAutomixSelector::getTopMatches(
        const TrackPointer& pFrom,
        const TrackAiFeatures& fromFeatures,
        const QList<QPair<TrackPointer, TrackAiFeatures>>& candidates,
        int maxResults) const {
    QList<TrackRecommendation> results;
    if (!pFrom || candidates.isEmpty()) {
        return results;
    }
    results.reserve(candidates.size());
    for (const auto& pair : candidates) {
        const TrackPointer& pTo = pair.first;
        if (!pTo || pTo == pFrom) {
            continue;
        }
        TransitionScoreBreakdown bd =
                computeScoresFromFeatures(pFrom, fromFeatures, pTo, pair.second);
        if (bd.rejected) {
            continue;
        }
        TrackRecommendation rec;
        rec.track = pTo;
        rec.scores = bd;
        rec.aiFeatures = pair.second;
        results.append(rec);
    }
    std::sort(results.begin(), results.end(),
            [](const TrackRecommendation& a, const TrackRecommendation& b) {
                return a.scores.overall > b.scores.overall;
            });
    if (results.size() > maxResults) {
        results.resize(maxResults);
    }
    return results;
}

TrackPointer AiAutomixSelector::selectBestNext(const TrackPointer& pCurrent,
        const QList<TrackPointer>& candidates) const {
    if (!mixxx::ai::isAutomixEnabled(m_pConfig)) {
        return TrackPointer();
    }
    if (!pCurrent || candidates.isEmpty()) {
        return TrackPointer();
    }

    // If a target genre is set, restrict to candidates of that genre when at
    // least one qualifies, so the requested genre change actually happens.
    QList<TrackPointer> pool = candidates;
    if (!m_targetGenre.isEmpty()) {
        QList<TrackPointer> matching;
        for (const TrackPointer& pCandidate : candidates) {
            if (pCandidate &&
                    storedGenre(pCandidate).compare(
                            m_targetGenre, Qt::CaseInsensitive) == 0) {
                matching.append(pCandidate);
            }
        }
        if (!matching.isEmpty()) {
            pool = matching;
        }
    }

    TrackPointer best;
    double bestScore = -1.0;
    for (const TrackPointer& pCandidate : pool) {
        if (!pCandidate || pCandidate == pCurrent) {
            continue;
        }
        const double score = scoreTransition(pCurrent, pCandidate);
        if (score > bestScore) {
            bestScore = score;
            best = pCandidate;
        }
    }
    if (best) {
        kLogger.debug() << "AI Automix picked" << best->getLocation()
                        << "score=" << bestScore;
    }
    return best;
}
