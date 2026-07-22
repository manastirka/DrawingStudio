#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

/** Authoritative metadata and basic validation for automation actions. */
class AutomationCommandCatalog {
public:
    static QJsonArray commands();
    static bool containsAction(const QString &action);
    static bool requiresFilesystemAccess(const QString &action);
    static QStringList missingRequiredParams(const QString &action,
                                             const QJsonObject &params);
};
