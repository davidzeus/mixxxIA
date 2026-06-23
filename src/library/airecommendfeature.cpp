#include "library/airecommendfeature.h"

#include <QFont>
#include <QHeaderView>
#include <QPalette>

#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "moc_airecommendfeature.cpp"
#include "track/track.h"
#include "widget/wlibrary.h"

namespace {
const QString kViewName = QStringLiteral("AI Suggest");
} // namespace

AiRecommendFeature::AiRecommendFeature(Library* pLibrary,
        UserSettingsPointer pConfig,
        AiTrackAdvisor* pAdvisor)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("ic_library_history.svg")),
          m_pAdvisor(pAdvisor),
          m_pModel(new AiRecommendModel(this)),
          m_pSidebarModel(make_parented<TreeItemModel>(this)) {
    auto pRoot = TreeItem::newRoot(this);
    m_pSidebarModel->setRootItem(std::move(pRoot));

    if (pAdvisor) {
        connect(pAdvisor,
                &AiTrackAdvisor::recommendationsUpdated,
                this,
                &AiRecommendFeature::onRecommendationsUpdated);
        connect(pAdvisor,
                &AiTrackAdvisor::currentTrackChanged,
                this,
                &AiRecommendFeature::onCurrentTrackChanged);
    }
}

QVariant AiRecommendFeature::title() {
    return tr("AI Suggest");
}

TreeItemModel* AiRecommendFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void AiRecommendFeature::bindLibraryWidget(
        WLibrary* libraryWidget, KeyboardEventFilter* /*keyboard*/) {
    auto* pView = new QWidget(libraryWidget);
    auto* pLayout = new QVBoxLayout(pView);
    pLayout->setContentsMargins(4, 4, 4, 4);
    pLayout->setSpacing(4);

    m_pHeader = new QLabel(tr("Load a track to see AI recommendations"), pView);
    QFont hFont = m_pHeader->font();
    hFont.setBold(true);
    m_pHeader->setFont(hFont);
    m_pHeader->setWordWrap(true);
    pLayout->addWidget(m_pHeader);

    m_pTable = new QTableView(pView);
    m_pTable->setModel(m_pModel);
    m_pTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pTable->setAlternatingRowColors(true);
    m_pTable->verticalHeader()->hide();
    m_pTable->horizontalHeader()->setStretchLastSection(false);
    m_pTable->horizontalHeader()->setSectionResizeMode(
            AiRecommendModel::ColTitle, QHeaderView::Stretch);
    m_pTable->horizontalHeader()->setSectionResizeMode(
            AiRecommendModel::ColArtist, QHeaderView::ResizeToContents);
    m_pTable->horizontalHeader()->setSectionResizeMode(
            AiRecommendModel::ColMatch, QHeaderView::ResizeToContents);
    m_pTable->horizontalHeader()->setSectionResizeMode(
            AiRecommendModel::ColBpm, QHeaderView::ResizeToContents);
    m_pTable->horizontalHeader()->setSectionResizeMode(
            AiRecommendModel::ColKey, QHeaderView::ResizeToContents);
    m_pTable->horizontalHeader()->setSectionResizeMode(
            AiRecommendModel::ColGenre, QHeaderView::ResizeToContents);
    m_pTable->horizontalHeader()->setSectionResizeMode(
            AiRecommendModel::ColMood, QHeaderView::ResizeToContents);
    m_pTable->horizontalHeader()->setSectionResizeMode(
            AiRecommendModel::ColEnergy, QHeaderView::ResizeToContents);
    connect(m_pTable,
            &QTableView::doubleClicked,
            this,
            &AiRecommendFeature::onTrackDoubleClicked);
    pLayout->addWidget(m_pTable);

    m_pView = pView;
    libraryWidget->registerView(kViewName, pView);
}

void AiRecommendFeature::activate() {
    emit switchToView(kViewName);
    emit disableSearch();
}

void AiRecommendFeature::onRecommendationsUpdated(QList<TrackRecommendation> recs) {
    m_pModel->setRecommendations(std::move(recs));
}

void AiRecommendFeature::onCurrentTrackChanged(
        QString title, double bpm, QString key) {
    if (!m_pHeader) {
        return;
    }
    QString label = tr("Based on: %1").arg(title);
    if (bpm > 0.0) {
        label += QStringLiteral(" · %1 BPM")
                         .arg(QString::number(bpm, 'f', 1));
    }
    if (!key.isEmpty()) {
        label += QStringLiteral(" · ") + key;
    }
    m_pHeader->setText(label);
}

void AiRecommendFeature::onTrackDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }
    const QVariant v = m_pModel->data(
            index.sibling(index.row(), 0), AiRecommendModel::TrackPointerRole);
    TrackPointer pTrack = v.value<TrackPointer>();
    if (pTrack) {
        emit loadTrack(pTrack);
    }
}
