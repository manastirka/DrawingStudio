#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QSettings>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("Drawing Studio");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Your Company");
    app.setApplicationDisplayName("Drawing Studio");

    // Emergency reset: clear QSettings to return UI to defaults
    // Usage: DrawingStudio --reset-settings
    if (app.arguments().contains("--reset-settings")) {
        QSettings s;
        s.clear();
        s.sync();
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
    MainWindow window;
    window.show();

    return app.exec();
}
