#include "ai/aisettingsdialog.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "ai/aisettings.h"

namespace {

struct ProviderItem {
    const char* label;
    const char* id;
};

// Order must stay in sync with the combobox population below.
const ProviderItem kProviders[] = {
        {"Local (Ollama, descarga automática)", "local"},
        {"Google (Gemini)", "google"},
        {"OpenAI (GPT)", "openai"},
        {"Anthropic (Claude)", "anthropic"},
};
constexpr int kProviderCount =
        static_cast<int>(sizeof(kProviders) / sizeof(kProviders[0]));

} // namespace

bool showAiSettingsDialog(QWidget* pParent, const UserSettingsPointer& pConfig) {
    QDialog dialog(pParent);
    dialog.setWindowTitle(QObject::tr("Ajustes de Automix IA"));

    auto* pProvider = new QComboBox(&dialog);
    int currentIndex = 0;
    const QString currentProvider = mixxx::ai::llmProvider(pConfig);
    for (int i = 0; i < kProviderCount; ++i) {
        pProvider->addItem(QObject::tr(kProviders[i].label),
                QString::fromLatin1(kProviders[i].id));
        if (currentProvider == QString::fromLatin1(kProviders[i].id)) {
            currentIndex = i;
        }
    }
    pProvider->setCurrentIndex(currentIndex);

    auto* pApiKey = new QLineEdit(&dialog);
    pApiKey->setEchoMode(QLineEdit::Password);
    pApiKey->setText(mixxx::ai::llmApiKey(pConfig));
    pApiKey->setPlaceholderText(
            QObject::tr("API key (no necesaria para Local)"));

    auto* pModel = new QLineEdit(&dialog);
    pModel->setText(mixxx::ai::llmModel(pConfig));
    pModel->setPlaceholderText(QObject::tr("(opcional — usa un valor por defecto)"));

    auto* pInfo = new QLabel(
            QObject::tr("El proveedor solo se usa para interpretar peticiones en "
                        "lenguaje natural (p. ej. «cambia a reggaetón»).\n"
                        "Los embeddings de audio siempre son locales y descargan "
                        "su modelo automáticamente; la GPU es opcional."),
            &dialog);
    pInfo->setWordWrap(true);

    // Enable/disable the API key field based on provider (local needs none).
    const auto updateApiKeyEnabled = [pProvider, pApiKey]() {
        pApiKey->setEnabled(pProvider->currentData().toString() !=
                QStringLiteral("local"));
    };
    updateApiKeyEnabled();
    QObject::connect(pProvider,
            &QComboBox::currentIndexChanged,
            pApiKey,
            [updateApiKeyEnabled](int) {
                updateApiKeyEnabled();
            });

    auto* pButtons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(pButtons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(pButtons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto* pForm = new QFormLayout();
    pForm->addRow(QObject::tr("Proveedor:"), pProvider);
    pForm->addRow(QObject::tr("API key:"), pApiKey);
    pForm->addRow(QObject::tr("Modelo:"), pModel);

    auto* pLayout = new QVBoxLayout(&dialog);
    pLayout->addLayout(pForm);
    pLayout->addWidget(pInfo);
    pLayout->addWidget(pButtons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    pConfig->setValue(
            ConfigKey(mixxx::ai::kConfigGroup, QStringLiteral("LlmProvider")),
            pProvider->currentData().toString());
    pConfig->setValue(
            ConfigKey(mixxx::ai::kConfigGroup, QStringLiteral("LlmApiKey")),
            pApiKey->text().trimmed());
    pConfig->setValue(
            ConfigKey(mixxx::ai::kConfigGroup, QStringLiteral("LlmModel")),
            pModel->text().trimmed());
    return true;
}
