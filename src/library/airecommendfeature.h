#pragma once

#include <QLabel>
#include <QObject>
#include <QPointer>
#include <QTableView>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include "library/airecommendmodel.h"
#include "library/aitrackadvisor.h"
#include "library/libraryfeature.h"
#include "library/treeitemmodel.h"
#include "util/parented_ptr.h"

class KeyboardEventFilter;
class Library;
class WLibrary;

/// "AI Suggest" library panel — shows the top tracks compatible with the
/// currently playing track, sorted by overall compatibility score.
class AiRecommendFeature : public LibraryFeature {
    Q_OBJECT
  public:
    AiRecommendFeature(Library* pLibrary,
            UserSettingsPointer pConfig,
            AiTrackAdvisor* pAdvisor);

    QVariant title() override;
    TreeItemModel* sidebarModel() const override;

    void bindLibraryWidget(
            WLibrary* libraryWidget, KeyboardEventFilter* keyboard) override;

  public slots:
    void activate() override;

  private slots:
    void onRecommendationsUpdated(QList<TrackRecommendation> recs);
    void onCurrentTrackChanged(QString title, double bpm, QString key);
    void onTrackDoubleClicked(const QModelIndex& index);

  private:
    AiTrackAdvisor* m_pAdvisor;
    AiRecommendModel* m_pModel;
    parented_ptr<TreeItemModel> m_pSidebarModel;

    // Owned by the WLibrary widget after registerView().
    QPointer<QWidget> m_pView;
    QPointer<QLabel> m_pHeader;
    QPointer<QTableView> m_pTable;
};
