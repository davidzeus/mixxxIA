#include "library/aitrackadvisor.h"

#include <QtConcurrent>

#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "moc_aitrackadvisor.cpp"
#include "util/logger.h"

namespace {
mixxx::Logger kLogger("AiTrackAdvisor");
constexpr int kMaxRecommendations = 10;
} // namespace

AiTrackAdvisor::AiTrackAdvisor(PlayerManagerInterface* pPlayerManager,
        TrackCollectionManager* pTrackCollectionManager,
        AiAutomixSelector* pSelector,
        QObject* parent)
        : QObject(parent),
          m_pPlayerManager(pPlayerManager),
          m_pTrackCollectionManager(pTrackCollectionManager),
          m_pSelector(pSelector) {
    if (!pPlayerManager) {
        return;
    }
    // Connect to track-loaded signals for all existing decks.
    const int numDecks = pPlayerManager->numberOfDecks();
    for (int i = 1; i <= numDecks; ++i) {
        BaseTrackPlayer* pDeck = pPlayerManager->getDeckBase(i);
        if (pDeck) {
            connect(pDeck,
                    &BaseTrackPlayer::newTrackLoaded,
                    this,
                    &AiTrackAdvisor::onTrackLoadedToDeck);
        }
    }
    // Wire up decks added at runtime (e.g. switching to 4-deck mode).
    connect(pPlayerManager,
            &PlayerManagerInterface::numberOfDecksChanged,
            this,
            [this](int newCount) {
                if (!m_pPlayerManager) {
                    return;
                }
                BaseTrackPlayer* pDeck = m_pPlayerManager->getDeckBase(newCount);
                if (pDeck) {
                    connect(pDeck,
                            &BaseTrackPlayer::newTrackLoaded,
                            this,
                            &AiTrackAdvisor::onTrackLoadedToDeck,
                            Qt::UniqueConnection);
                }
            });
}

void AiTrackAdvisor::onTrackLoadedToDeck(TrackPointer pTrack) {
    if (!pTrack) {
        return;
    }
    m_pCurrentTrack = pTrack;
    const QString keyText = KeyUtils::keyToString(pTrack->getKey());
    emit currentTrackChanged(pTrack->getTitle(), pTrack->getBpm(), keyText);

    if (!m_pSelector) {
        return;
    }

    // Fetch all data on the main thread (DAO has thread-affinity checks).
    TrackAiFeatures fromFeatures;
    auto candidates = fetchCandidates(pTrack, &fromFeatures);

    // Scoring is pure arithmetic — safe to run on a background thread.
    AiAutomixSelector* pSelector = m_pSelector;
    QtConcurrent::run([pTrack, fromFeatures, candidates, pSelector, this]() {
        QList<TrackRecommendation> recs = pSelector->getTopMatches(
                pTrack, fromFeatures, candidates, kMaxRecommendations);
        kLogger.debug() << "AiTrackAdvisor: computed" << recs.size()
                        << "recommendations for" << pTrack->getTitle();
        emit recommendationsUpdated(recs);
    });
}

QList<QPair<TrackPointer, TrackAiFeatures>> AiTrackAdvisor::fetchCandidates(
        const TrackPointer& pCurrent,
        TrackAiFeatures* pFromFeatures) const {
    QList<QPair<TrackPointer, TrackAiFeatures>> result;
    if (!m_pTrackCollectionManager) {
        return result;
    }
    TrackCollection* pCollection = m_pTrackCollectionManager->internalCollection();
    if (!pCollection) {
        return result;
    }
    AiFeatureDao& dao = pCollection->getAiFeatureDAO();

    // Pre-fetch from-features for the current track.
    if (pFromFeatures && pCurrent && pCurrent->getId().isValid()) {
        dao.getFeatures(pCurrent->getId(), pFromFeatures);
    }

    const QList<TrackAiFeatures> allFeatures = dao.getAllFeatures();
    result.reserve(allFeatures.size());
    for (const TrackAiFeatures& feat : allFeatures) {
        if (!feat.trackId.isValid()) {
            continue;
        }
        TrackPointer pTrack = m_pTrackCollectionManager->getTrackById(feat.trackId);
        if (!pTrack || pTrack == pCurrent) {
            continue;
        }
        result.append(qMakePair(pTrack, feat));
    }
    return result;
}
