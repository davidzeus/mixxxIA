#pragma once

#include <QList>
#include <QObject>
#include <QPair>

#include "library/autodj/aiautomixselector.h"
#include "track/track_decl.h"

class PlayerManagerInterface;
class TrackCollectionManager;

/// Watches the active deck and emits ranked track recommendations whenever
/// the playing track changes.
///
/// Candidates (track pointers + AI features) are fetched on the main thread,
/// then the pure arithmetic scoring runs on a background thread so the UI is
/// never blocked.
class AiTrackAdvisor : public QObject {
    Q_OBJECT
  public:
    AiTrackAdvisor(PlayerManagerInterface* pPlayerManager,
            TrackCollectionManager* pTrackCollectionManager,
            AiAutomixSelector* pSelector,
            QObject* parent = nullptr);

  signals:
    void recommendationsUpdated(QList<TrackRecommendation> recs);
    void currentTrackChanged(QString title, double bpm, QString key);

  private slots:
    void onTrackLoadedToDeck(TrackPointer pTrack);

  private:
    /// Fetch all library tracks that have AI features, excluding pExclude.
    /// Also fills *pFromFeatures with the AI features for pCurrent (or empty).
    /// Must be called on the main thread.
    QList<QPair<TrackPointer, TrackAiFeatures>> fetchCandidates(
            const TrackPointer& pCurrent,
            TrackAiFeatures* pFromFeatures) const;

    PlayerManagerInterface* m_pPlayerManager;
    TrackCollectionManager* m_pTrackCollectionManager;
    AiAutomixSelector* m_pSelector;
    TrackPointer m_pCurrentTrack;
};
