#include <QApplication>
#include <QStyleFactory>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QSettings>
#include <QTextStream>
#include <QUuid>
#include "CommandServer.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("Drawing Studio");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Your Company");
    app.setApplicationDisplayName("Drawing Studio");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Drawing Studio"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption resetSettingsOption(
        QStringLiteral("reset-settings"),
        QStringLiteral("Clear saved UI settings before startup."));
    const QCommandLineOption enableAutomationOption(
        QStringLiteral("enable-automation"),
        QStringLiteral("Enable the authenticated localhost automation API."));
    const QCommandLineOption automationTokenOption(
        QStringLiteral("automation-token"),
        QStringLiteral("Bearer token for the automation API. A random token is generated if omitted."),
        QStringLiteral("token"));
    const QCommandLineOption automationPortOption(
        QStringLiteral("automation-port"),
        QStringLiteral("Local automation API port (default: 19100)."),
        QStringLiteral("port"), QStringLiteral("19100"));
    const QCommandLineOption automationFilesystemOption(
        QStringLiteral("automation-allow-filesystem"),
        QStringLiteral("Allow automation commands to open, import, save, and export files."));
    parser.addOptions({resetSettingsOption, enableAutomationOption, automationTokenOption,
                       automationPortOption, automationFilesystemOption});
    parser.process(app);

    // Emergency reset: clear QSettings to return UI to defaults
    // Usage: DrawingStudio --reset-settings
    if (parser.isSet(resetSettingsOption)) {
        QSettings s;
        s.clear();
        s.sync();
    }

    MainWindow::AutomationOptions automation;
    automation.enabled = parser.isSet(enableAutomationOption);
    automation.allowFilesystemCommands = parser.isSet(automationFilesystemOption);

    if (!automation.enabled
        && (parser.isSet(automationTokenOption)
            || parser.isSet(automationFilesystemOption)
            || parser.isSet(automationPortOption))) {
        QTextStream(stderr)
            << "Automation options require --enable-automation.\n";
        return 2;
    }

    if (automation.enabled) {
        bool portOk = false;
        const uint requestedPort = parser.value(automationPortOption).toUInt(&portOk);
        if (!portOk || requestedPort == 0 || requestedPort > 65535) {
            QTextStream(stderr) << "Invalid --automation-port value.\n";
            return 2;
        }
        automation.port = static_cast<quint16>(requestedPort);
        automation.token = parser.value(automationTokenOption).trimmed();
        if (automation.token.isEmpty())
            automation.token = qEnvironmentVariable("DRAWINGSTUDIO_AUTOMATION_TOKEN").trimmed();
        if (automation.token.isEmpty()) {
            automation.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
            QTextStream(stderr)
                << "DrawingStudio automation token: " << automation.token << "\n";
        }
        if (automation.token.toUtf8().size() < CommandServer::kMinAuthTokenBytes) {
            QTextStream(stderr)
                << "Automation token must be at least "
                << CommandServer::kMinAuthTokenBytes << " bytes.\n";
            return 2;
        }
        QTextStream(stderr)
            << "Automation API enabled on http://127.0.0.1:" << automation.port
            << (automation.allowFilesystemCommands
                    ? " with filesystem commands enabled\n"
                    : " with filesystem commands disabled\n");
    }

    // Set a modern style
    app.setStyle(QStyleFactory::create("Fusion"));

    // Apply dark theme (optional)
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(70, 70, 70));
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(darkPalette);

    // Create and show main window
    MainWindow window(automation, nullptr);
    window.show();

    return app.exec();
}
