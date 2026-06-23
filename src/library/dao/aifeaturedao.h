#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

#include "library/dao/dao.h"
#include "track/trackid.h"

/// Per-track AI analysis results: an audio embedding plus high-level tags
/// (genre / mood / energy / danceability) produced by the AI sidecar.
/// Used by the AI Automix selector to find tracks that sound similar to the
/// one currently playing.
struct TrackAiFeatures {
    TrackId trackId;
    QString modelVersion;
    /// L2-normalized float32 audio embedding.
    QVector<float> embedding;
    QString genre;
    double genreConfidence = 0.0;
    QString mood;
    double energy = 0.0;
    double danceability = 0.0;
    /// Unix epoch (seconds) when the track was analyzed; 0 if unknown.
    qint64 analyzedAt = 0;

    bool isValid() const {
        return trackId.isValid() && !embedding.isEmpty();
    }
};

class AiFeatureDao : public DAO {
  public:
    static const QString kTableName;

    AiFeatureDao() = default;
    ~AiFeatureDao() override = default;

    /// Insert or replace the AI features for a track. Returns false on error.
    bool saveFeatures(const TrackAiFeatures& features);

    /// Load AI features for a single track. Returns false if absent/error.
    bool getFeatures(TrackId trackId, TrackAiFeatures* pFeatures) const;

    /// Load every track's AI features (used to build the in-memory search
    /// index for the Automix selector).
    QList<TrackAiFeatures> getAllFeatures() const;

    /// True if the track already has stored AI features.
    bool hasFeatures(TrackId trackId) const;

    /// Distinct non-empty genres present across analyzed tracks, sorted.
    /// Used to populate the AI Automix genre selector so the user can only
    /// target genres that actually exist in the library.
    QStringList getDistinctGenres() const;

    /// Remove AI features for a track (e.g. when the track is deleted).
    bool deleteFeatures(TrackId trackId);
    void deleteFeaturesForTracks(const QList<TrackId>& trackIds);

    /// Serialization helpers (BLOB <-> float vector), exposed for testing.
    static QByteArray embeddingToBlob(const QVector<float>& embedding);
    static QVector<float> blobToEmbedding(const QByteArray& blob);
};
