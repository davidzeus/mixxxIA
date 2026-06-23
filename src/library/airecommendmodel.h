#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QString>

#include "library/autodj/aiautomixselector.h"

/// Table model that backs the "AI Suggest" recommendation panel.
/// Columns: Title, Artist, Match%, BPM%, Key%, Genre, Mood, Energy
class AiRecommendModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column {
        ColTitle = 0,
        ColArtist,
        ColMatch,
        ColBpm,
        ColKey,
        ColGenre,
        ColMood,
        ColEnergy,
        ColCount
    };

    enum Role {
        OverallScoreRole = Qt::UserRole + 1,  ///< double 0..1 (for coloring)
        TrackPointerRole = Qt::UserRole + 2,  ///< TrackPointer (for load)
    };

    explicit AiRecommendModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
            Qt::Orientation orientation,
            int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

  public slots:
    void setRecommendations(QList<TrackRecommendation> recs);

  private:
    QList<TrackRecommendation> m_recs;
};
