#include "library/dao/aifeaturedao.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <cstring>

#include "library/queryutil.h"
#include "util/logger.h"

namespace {
mixxx::Logger kLogger("AiFeatureDao");
} // namespace

const QString AiFeatureDao::kTableName = QStringLiteral("track_ai_features");

// static
QByteArray AiFeatureDao::embeddingToBlob(const QVector<float>& embedding) {
    QByteArray blob;
    blob.resize(static_cast<int>(embedding.size() * sizeof(float)));
    if (!embedding.isEmpty()) {
        std::memcpy(blob.data(), embedding.constData(), blob.size());
    }
    return blob;
}

// static
QVector<float> AiFeatureDao::blobToEmbedding(const QByteArray& blob) {
    QVector<float> embedding;
    const int count = static_cast<int>(blob.size() / sizeof(float));
    if (count <= 0) {
        return embedding;
    }
    embedding.resize(count);
    std::memcpy(embedding.data(), blob.constData(), count * sizeof(float));
    return embedding;
}

bool AiFeatureDao::saveFeatures(const TrackAiFeatures& features) {
    if (!m_database.isOpen() || !features.isValid()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "REPLACE INTO %1 (track_id, model_version, embedding, "
            "embedding_dim, genre, genre_confidence, mood, energy, "
            "danceability, analyzed_at) VALUES (:track_id, :model_version, "
            ":embedding, :embedding_dim, :genre, :genre_confidence, :mood, "
            ":energy, :danceability, :analyzed_at)")
                          .arg(kTableName));
    query.bindValue(":track_id", features.trackId.toVariant());
    query.bindValue(":model_version", features.modelVersion);
    query.bindValue(":embedding", embeddingToBlob(features.embedding));
    query.bindValue(":embedding_dim", features.embedding.size());
    query.bindValue(":genre", features.genre.isEmpty() ? QVariant() : features.genre);
    query.bindValue(":genre_confidence", features.genreConfidence);
    query.bindValue(":mood", features.mood.isEmpty() ? QVariant() : features.mood);
    query.bindValue(":energy", features.energy);
    query.bindValue(":danceability", features.danceability);
    query.bindValue(":analyzed_at", features.analyzedAt);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Failed to save AI features for track"
                                << features.trackId;
        return false;
    }
    return true;
}

bool AiFeatureDao::getFeatures(TrackId trackId, TrackAiFeatures* pFeatures) const {
    if (!m_database.isOpen() || !trackId.isValid() || !pFeatures) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT model_version, embedding, genre, genre_confidence, mood, "
            "energy, danceability, analyzed_at FROM %1 WHERE track_id=:track_id")
                          .arg(kTableName));
    query.bindValue(":track_id", trackId.toVariant());

    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Failed to load AI features for track" << trackId;
        return false;
    }
    if (!query.next()) {
        return false;
    }

    pFeatures->trackId = trackId;
    pFeatures->modelVersion = query.value(0).toString();
    pFeatures->embedding = blobToEmbedding(query.value(1).toByteArray());
    pFeatures->genre = query.value(2).toString();
    pFeatures->genreConfidence = query.value(3).toDouble();
    pFeatures->mood = query.value(4).toString();
    pFeatures->energy = query.value(5).toDouble();
    pFeatures->danceability = query.value(6).toDouble();
    pFeatures->analyzedAt = query.value(7).toLongLong();
    return pFeatures->isValid();
}

QList<TrackAiFeatures> AiFeatureDao::getAllFeatures() const {
    QList<TrackAiFeatures> result;
    if (!m_database.isOpen()) {
        return result;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT track_id, model_version, embedding, genre, "
            "genre_confidence, mood, energy, danceability, analyzed_at FROM %1")
                          .arg(kTableName));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Failed to load all AI features";
        return result;
    }
    while (query.next()) {
        TrackAiFeatures f;
        f.trackId = TrackId(query.value(0));
        f.modelVersion = query.value(1).toString();
        f.embedding = blobToEmbedding(query.value(2).toByteArray());
        f.genre = query.value(3).toString();
        f.genreConfidence = query.value(4).toDouble();
        f.mood = query.value(5).toString();
        f.energy = query.value(6).toDouble();
        f.danceability = query.value(7).toDouble();
        f.analyzedAt = query.value(8).toLongLong();
        if (f.isValid()) {
            result.append(f);
        }
    }
    return result;
}

bool AiFeatureDao::hasFeatures(TrackId trackId) const {
    if (!m_database.isOpen() || !trackId.isValid()) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT 1 FROM %1 WHERE track_id=:track_id LIMIT 1")
                          .arg(kTableName));
    query.bindValue(":track_id", trackId.toVariant());
    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Failed to check AI features for track" << trackId;
        return false;
    }
    return query.next();
}

QStringList AiFeatureDao::getDistinctGenres() const {
    QStringList genres;
    if (!m_database.isOpen()) {
        return genres;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT DISTINCT genre FROM %1 WHERE genre IS NOT NULL AND "
            "genre <> '' ORDER BY genre COLLATE NOCASE")
                          .arg(kTableName));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Failed to load distinct AI genres";
        return genres;
    }
    while (query.next()) {
        genres.append(query.value(0).toString());
    }
    return genres;
}

bool AiFeatureDao::deleteFeatures(TrackId trackId) {
    if (!m_database.isOpen() || !trackId.isValid()) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM %1 WHERE track_id=:track_id")
                          .arg(kTableName));
    query.bindValue(":track_id", trackId.toVariant());
    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Failed to delete AI features for track" << trackId;
        return false;
    }
    return true;
}

void AiFeatureDao::deleteFeaturesForTracks(const QList<TrackId>& trackIds) {
    if (!m_database.isOpen() || trackIds.isEmpty()) {
        return;
    }
    QStringList idList;
    idList.reserve(trackIds.size());
    for (const auto& trackId : trackIds) {
        if (trackId.isValid()) {
            idList << trackId.toString();
        }
    }
    if (idList.isEmpty()) {
        return;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM %1 WHERE track_id IN (%2)")
                          .arg(kTableName, idList.join(QChar(','))));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Failed to delete AI features for tracks";
    }
}
