#pragma once

#include <QList>

#include "preferences/usersettings.h"
#include "track/track_decl.h"

class TrackCollectionManager;

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

  private:
    UserSettingsPointer m_pConfig;
    TrackCollectionManager* m_pTrackCollectionManager;
};
