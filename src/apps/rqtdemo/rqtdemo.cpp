/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

/**
* This is an example of embedding Rocky in a Qt application.
*/

#include <rocky/vsg/Application.h>
#include <rocky/Version.h>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>

#include <vsgQt/Window.h>

#ifdef ROCKY_HAS_TMS
#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>
#endif

ROCKY_ABOUT(qt, qVersion())

using namespace ROCKY_NAMESPACE;

template<class T>
int layerError(T layer)
{
    rocky::Log()->warn("Problem with layer \"" + layer->name() + "\" : " + layer->status().message);
    return -1;
}

//! Specialize the osgQt::Viewer to interoperate with the Rocky Application object
class MyQtViewer : public vsg::Inherit<vsgQt::Viewer, MyQtViewer>
{
public:
    void render() override
    {
        if (continuousUpdate || requests.load() > 0)
        {
            if (frame() == false)
            {
                if (status->cancel())
                {
                    QApplication::quit();
                }
            }
        }
    }

    std::function<bool()> frame;
};


int main(int argc, char** argv)
{
    QApplication qt_app(argc, argv);

    rocky::Application app(argc, argv);

    rocky::Log()->set_level(rocky::log::level::info);

    // First, create a customized Qt-based viewer and integrate it with our Rocky Application object.
    auto viewer = MyQtViewer::create();
    viewer->frame = [&app]() { return app.frame(); };
    viewer->continuousUpdate = true;
    app.setViewer(viewer);

    // Set up the main window and its central container widget as you normally would in Qt.
    // We also set the content margins for a nicer window-filling look.
    QMainWindow mainWindow;
    mainWindow.setGeometry(0, 0, 800, 600);
    mainWindow.setWindowTitle("Rocky Qt Example");
    auto centralWidget = new QWidget(&mainWindow);
    mainWindow.setCentralWidget(centralWidget);
    auto layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(1,0,1,1);

    // Add a simple menu bar.
    auto menubar = mainWindow.menuBar();
    auto filemenu = menubar->addMenu("&File");
    filemenu->addAction("E&xit", &qt_app, &QApplication::quit);

    // Create a Qt container for our Rocky widget, and add it to the layout.
    auto rocky_window = new vsgQt::Window();
    auto rocky_widget = QWidget::createWindowContainer(rocky_window);
    layout->addWidget(rocky_widget);

    // Initialize the Vulkan widget.
    // NB: You must call this AFTER calling QWidget::createWindowContainer
    // otherwise the Qt layout will not work properly.
    rocky_window->initializeWindow();

    // Finally add it to the Rocky display manager.
    app.displayManager->addWindow(rocky_window->windowAdapter);

    // Add some data to the map if necessary.
    if (app.mapNode->map->layers().empty())
    {
#ifdef ROCKY_HAS_TMS
        // add an imagery layer to the map
        auto layer = rocky::TMSImageLayer::create();
        layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7";
        app.mapNode->map->layers().add(layer);
        if (layer->status().failed())
            return layerError(layer);
#endif
    }

    // Run until the user quits.
    mainWindow.show();
    return qt_app.exec();
}
