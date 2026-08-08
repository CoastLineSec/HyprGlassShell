#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include <cstdlib>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("hyprshelld-settings"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("HyprShelld Settings"));
    QGuiApplication::setOrganizationName(QStringLiteral("CoastLineSec"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection
    );
    engine.loadFromModule("HyprShelld.Settings", "Main");

    return application.exec();
}
