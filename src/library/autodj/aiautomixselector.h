#pragma once

#include <QList>
#include <QPair>

#include "library/dao/aifeaturedao.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class TrackCollectionManager;

/// Per-dimension score breakdown for a single track transition.
/// Returned by scoreTransitionDetailed() and used by the AI Suggest panel.
struct TransitionScoreBreakdown {
    double overall = -1.0;        ///< Weighted total (0..1) or -1.0 if rejected
    double bpmScore = 0.0;        ///< BPM closeness (0..1)
    double keyScore = 0.0;        ///< Harmonic key compatibility (0..1)
    double embeddingScore = 0.0;  ///< Audio embedding similarity (0..1)
    double energyScore = 0.0;     ///< Energy continuity (0..1)
    bool rejected = false;        ///< True when BPM gap exceeds maxBpmFraction
};

/// A single recommendation entry: track + per-dimension scores + AI metadata.
struct TrackRecommendation {
    TrackPointer track;
    TransitionScoreBreakdown scores;
    TrackAiFeatures aiFeatures;
};

/// Picks the best "next" track for AI Automix.
///
/// Given the track currently playing and a pool of candidate tracks (the
/// AutoDJ queue), it scores each candidate by combining:
///   * audio-embedding cosine similarity (how similar they sound),
///   * harmonic key compatibility (Camelot / Circle of Fifths),
///   * BPM closeness,
///   * energy continuity.
/// Candidates whose BPM is too far from the current track are rejected
/// outright. The highest-scoring candidate is returned.
///
/// Tracks without stored AI features still participate using the
/// key/BPM/energy terms, so the feature degrades gracefully.
class AiAutomixSelector {
  public:
    AiAutomixSelector(UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager);

    /// Returns the candidate that best follows pCurrent, or a null pointer if
    /// AI Automix is disabled, the inputs are empty, or none qualifies.
    TrackPointer selectBestNext(const TrackPointer& pCurrent,
            const QList<TrackPointer>& candidates) const;

    /// 0..1 compatibility score for a single transition (exposed for UI/debug
    /// and testing). Returns -1.0 if the transition is rejected (e.g. BPM too
    /// far apart).
    double scoreTransition(const TrackPointer& pFrom, const TrackPointer& pTo) const;

    /// Full per-dimension breakdown for a single transition.
    /// Useful for display in the AI Suggest panel.
    TransitionScoreBreakdown scoreTransitionDetailed(
            const TrackPointer& pFrom, const TrackPointer& pTo) const;

    /// Score all candidates against pFrom and return the top maxResults,
    /// sorted by overall score descending.  Rejected transitions are excluded.
    /// fromFeatures must be pre-fetched by the caller (e.g. on the main thread)
    /// so this method is safe to call from any thread.
    QList<TrackRecommendation> getTopMatches(
            const TrackPointer& pFrom,
            const TrackAiFeatures& fromFeatures,
            const QList<QPair<TrackPointer, TrackAiFeatures>>& candidates,
            int maxResults = 10) const;

    /// Steer Automix toward a genre (e.g. set from the genre selector or a
    /// natural-language request). Empty string clears the target. When set,
    /// candidates of that genre are strongly preferred.
    void setTargetGenre(const QString& genre);
    QString targetGenre() const {
        return m_targetGenre;
    }

  private:
    /// Core scoring implementation — returns a full breakdown.
    TransitionScoreBreakdown computeScores(
            const TrackPointer& pFrom, const TrackPointer& pTo) const;

    /// Thread-safe variant that uses pre-fetched features (no DAO access).
    TransitionScoreBreakdown computeScoresFromFeatures(
            const TrackPointer& pFrom,
            const TrackAiFeatures& fromFeat,
            const TrackPointer& pTo,
            const TrackAiFeatures& toFeat) const;

    /// Stored AI genre for a track, or empty if none.
    QString storedGenre(const TrackPointer& pTrack) const;

    UserSettingsPointer m_pConfig;
    TrackCollectionManager* m_pTrackCollectionManager;
    QString m_targetGenre;
};
