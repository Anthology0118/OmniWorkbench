#include "../generated/embedded_ui.hpp"
#include "../server/app_server.hpp"

#include <QApplication>
#include <QMainWindow>
#include <QUrl>
#include <QWebEngineView>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("OmniWorkbench");

    omni::AppServer server(omni::get_embedded_ui_html());
    if (!server.start(0)) {
        return 1;
    }

    QMainWindow win;
    win.setWindowTitle("OmniWorkbench (全能工作台)");
    win.resize(1380, 860);

    auto* view = new QWebEngineView(&win);
    view->setUrl(QUrl(QString("http://127.0.0.1:%1/").arg(server.port())));
    win.setCentralWidget(view);
    win.show();

    int rc = app.exec();
    server.stop();
    return rc;
}
