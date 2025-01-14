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
    void render(double simTime) override
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

//! Custom event filter to remove a window from the display manager when the Qt window is closed
class CloseQtWindowEventFilter : public QObject
{
public:
    CloseQtWindowEventFilter(
        std::shared_ptr<rocky::DisplayManager> dm_,
        vsg::ref_ptr<vsg::Window> window_,
        QObject* parent =nullptr) : dm(dm_), window(window_), QObject(parent) { }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::Close && dm && window)
        {
            Log()->info("Captured close event for window " + window->traits()->windowTitle);
            dm->removeWindow(window);
        }
        return QObject::eventFilter(obj, event);
    }

    std::shared_ptr<rocky::DisplayManager> dm;
    vsg::ref_ptr<vsg::Window> window;
};

//! Add a new window to the application
void newWindow(rocky::Application& app)
{
    auto dm = app.displayManager;
    app.onNextUpdate([dm]()
        {
            auto i = dm->windowsAndViews.size();

            // the window:
            auto window = new QWidget();
            std::string name = "RockyQt - Window #" + std::to_string(i + 1);
            window->setWindowTitle(name.c_str());
            window->setGeometry(50, 50, 800, 600);

            // share the vk device with the main window:
            auto traits = vsg::WindowTraits::create();
            traits->device = dm->sharedDevice();
            auto rocky_window = new vsgQt::Window(traits);

            // wrap the rocky view in a widget:
            auto rocky_widget = QWidget::createWindowContainer(rocky_window);
            auto layout = new QVBoxLayout(window);
            layout->setContentsMargins(1, 0, 1, 1);
            layout->addWidget(rocky_widget);

            // fire it up:
            rocky_window->initializeWindow();

            // register with our display manager:
            dm->addWindow(rocky_window->windowAdapter);

            // intercept the close event to remove the window from the display manager:
            window->installEventFilter(new CloseQtWindowEventFilter(dm, rocky_window->windowAdapter));

            window->show();
        });
}

int main(int argc, char** argv)
{
    QApplication qt_app(argc, argv);

    // First, create a customized Qt-based viewer and integrate it with our Rocky Application object.
    auto viewer = MyQtViewer::create();

    rocky::Application app(viewer, argc, argv);
    app.context->renderContinuously = true;

    viewer->frame = [&app]() { return app.frame(); };
    viewer->continuousUpdate = true;
    viewer->setInterval(8);

    rocky::Log()->set_level(rocky::log::level::info);

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
    filemenu->addAction("&New Window", [&app]() { newWindow(app); });
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
#endif
    }

    // Run until the user quits.
    mainWindow.show();
    return qt_app.exec();
}
