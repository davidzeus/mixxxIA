#pragma once

#include "preferences/usersettings.h"

class QWidget;

/// Show a small modal dialog to configure the AI natural-language provider
/// (local Ollama or a cloud provider + API key) and persist it to the "[AI]"
/// config group. Built entirely in code (no .ui / no QObject subclass) to stay
/// decoupled from the preferences framework.
///
/// Returns true if the user accepted and settings were saved.
bool showAiSettingsDialog(QWidget* pParent, const UserSettingsPointer& pConfig);
