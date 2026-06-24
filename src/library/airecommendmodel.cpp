#include "library/airecommendmodel.h"

#include "moc_airecommendmodel.cpp"
#include "track/keyutils.h"
#include "track/track.h"


AiRecommendModel::AiRecommendModel(QObject* parent)
        : QAbstractTableModel(parent) {
}

int AiRecommendModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_recs.size();
}

int AiRecommendModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return ColCount;
}

QVariant AiRecommendModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_recs.size()) {
        return QVariant();
    }
    const TrackRecommendation& rec = m_recs.at(index.row());
    if (!rec.track) {
        return QVariant();
    }

    if (role == OverallScoreRole) {
        return rec.scores.overall;
    }
    if (role == TrackPointerRole) {
        return QVariant::fromValue(rec.track);
    }
    if (role == Qt::TextAlignmentRole) {
        if (index.column() >= ColMatch) {
            return QVariant(Qt::AlignCenter);
        }
        return QVariant();
    }
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    const auto pct = [](double v) -> QString {
        return QString::number(static_cast<int>(std::round(v * 100))) + '%';
    };

    switch (index.column()) {
    case ColTitle:
        return rec.track->getTitle();
    case ColArtist:
        return rec.track->getArtist();
    case ColMatch:
        return pct(rec.scores.overall);
    case ColBpm: {
        const double bpm = rec.track->getBpm();
        const QString bpmStr = bpm > 0.0
                ? QString::number(bpm, 'f', 1)
                : QStringLiteral("—");
        return QString(bpmStr + QStringLiteral(" · ") + pct(rec.scores.bpmScore));
    }
    case ColKey: {
        const QString keyStr =
                KeyUtils::keyToString(rec.track->getKey());
        return QString((keyStr.isEmpty() ? QStringLiteral("—") : keyStr) +
                QStringLiteral(" · ") + pct(rec.scores.keyScore));
    }
    case ColGenre:
        return rec.aiFeatures.genre;
    case ColMood:
        return rec.aiFeatures.mood;
    case ColEnergy:
        if (rec.aiFeatures.energy > 0.0) {
            return QString::number(rec.aiFeatures.energy, 'f', 2);
        }
        return QStringLiteral("—");
    default:
        return QVariant();
    }
}

QVariant AiRecommendModel::headerData(
        int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }
    switch (section) {
    case ColTitle:  return tr("Title");
    case ColArtist: return tr("Artist");
    case ColMatch:  return tr("Match");
    case ColBpm:    return tr("BPM");
    case ColKey:    return tr("Key");
    case ColGenre:  return tr("Genre");
    case ColMood:   return tr("Mood");
    case ColEnergy: return tr("Energy");
    default:        return QVariant();
    }
}

Qt::ItemFlags AiRecommendModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void AiRecommendModel::setRecommendations(QList<TrackRecommendation> recs) {
    beginResetModel();
    m_recs = std::move(recs);
    endResetModel();
}
