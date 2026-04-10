/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/Callbacks.h>
#include <vector>
#include <deque>

namespace ROCKY_NAMESPACE
{
    class DisplayManager;
    class MapManipulator;
    class Window;


    class ROCKY_EXPORT View
    {
    public:
        //! Construct an empty, invalid view
        View() = default;

        //! Locates the first occurance of a node of type T in the view's scene graph
        //! Example: auto mapNode = view.find<MapNode>();
        template<class T> T* find() {
            return detail::find<T>(vsgView);
        }

        //! Call this when you resize or change the properties of the underlying VSG view.
        void dirty();

        //! Validity operator
        operator bool() const {
            return vsgView != nullptr;
        }

        //! Equality operator
        bool operator == (const View& rhs) const {
            return vsgView == rhs.vsgView;
        }

        ViewIDType viewID = 0;
        vsg::ref_ptr<vsg::View> vsgView = nullptr;
        vsg::ref_ptr<vsg::RenderGraph> renderGraph = nullptr;

    protected:

        View(vsg::ref_ptr<vsg::View>, vsg::ref_ptr<vsg::RenderGraph>, DisplayManager*);

        vsg::ref_ptr<vsg::Group> _guiRenderer;
        vsg::ref_ptr<vsg::Visitor> _guiEventVisitor;
        std::shared_ptr<std::function<void()>> _guiIdleEventProcessor;
        const DisplayManager* _display = nullptr;

        friend class Window;
        friend class DisplayManager;
        friend class Application;
    };


    class ViewsFrontToBack
    {
    public:
        using container = std::deque<View>;
        using iterator = container::iterator;
        using const_iterator = container::const_iterator;
        using reverse_iterator = container::reverse_iterator;
        using const_reverse_iterator = container::const_reverse_iterator;

    public:
        ViewsFrontToBack() = default;

        std::size_t size() const {
            return _container.size();
        }

        View& operator [] (std::size_t index) {
            return _container[index];
        }

        const View& operator [] (std::size_t index) const {
            return _container[index];
        }

    public:
        iterator begin() { return _container.begin(); }
        iterator end() { return _container.end(); }
        reverse_iterator rbegin() { return _container.rbegin(); }
        reverse_iterator rend() { return _container.rend(); }
        const_iterator begin() const { return _container.begin(); }
        const_iterator end() const { return _container.end(); }
        const_reverse_iterator rbegin() const { return _container.rbegin(); }
        const_reverse_iterator rend() const { return _container.rend(); }
        View& front() { return _container.front(); }
        const View& front() const { return _container.front(); }
        View& back() { return _container.back(); }
        const View& back() const { return _container.back(); }
        bool empty() const { return _container.empty(); }

    private:
        container _container;

        iterator find(const View& view) {
            return std::find_if(_container.begin(), _container.end(), [&](const View& v) { return v == view; });
        }

        friend class Window;
        friend class DisplayManager;
    };


    class ROCKY_EXPORT Window
    {
    public:
        Window() = default;

        Window(const Window& rhs) = default;

        View& addView(vsg::ref_ptr<vsg::Camera>, vsg::ref_ptr<vsg::Node>);

        View& addView(vsg::ref_ptr<vsg::View>);

        void removeView(View&);

        ViewsFrontToBack& views() { return *_views; }
        const ViewsFrontToBack& views() const { return *_views; }
        
        View& view(std::size_t index = 0) {
            return _views->operator[](_views->size() - 1 - index);
        }
        const View& view(std::size_t index = 0) const {
            return _views->operator[](_views->size() - 1 - index);
        }

        View& viewAtCoords(float x, float y);

        //! valid window?
        operator bool() const {
            return vsgWindow && commandGraph && _display;
        }

        vsg::ref_ptr<vsg::Window> vsgWindow;
        vsg::ref_ptr<vsg::CommandGraph> commandGraph;

        bool operator == (const Window& rhs) const {
            return vsgWindow == rhs.vsgWindow;
        }

    private:
        Window(vsg::ref_ptr<vsg::Window>, vsg::ref_ptr<vsg::CommandGraph>, DisplayManager*);

        std::shared_ptr<ViewsFrontToBack> _views = std::make_shared<ViewsFrontToBack>();
        DisplayManager* _display = nullptr;

        friend class DisplayManager;
    };


    /**
    * DisplayManager is a helper class that manages the creation and destruction of
    * windows and views. It also provides some utility methods for working with
    * render graphcs an manipulators.
    */
    class ROCKY_EXPORT DisplayManager
    {
    public:
        //! Construct an empty display manager.
        //! Call initialize() later to set it up.
        DisplayManager() = default;

        // Destructor
        ~DisplayManager();

        //! Construct a display manager connected to a VSG context (and associated viewer).
        //! Use this if your app doesn't use the rocky Application object.
        void initialize(VSGContext);
        void initialize(VSGContext, vsg::CommandLine& commandLine);

        Window& window(std::size_t index = 0);
        const Window& window(std::size_t index = 0) const;
        std::vector<Window>& windows();
        const std::vector<Window>& windows() const;

        //! Creates a new window and adds it to the display, and also creates a
        //! full-window View for the new window.
        //! @param traits Window traits used to create the new window
        Window& addWindow(vsg::ref_ptr<vsg::WindowTraits>);

        //! Creates a new window and adds it to the display, with an existing view
        Window& addWindow(vsg::ref_ptr<vsg::Window>, vsg::ref_ptr<vsg::View>);

        //! Removes a window from the display.
        //! #param window Window to remove
        void removeWindow(const Window& window);

        //! Given a VSG window pointer, find the corresponding DisplayManager Window.
        Window& find(vsg::Window*);

        //! Given a VSG View pointer, find the corresponding DisplayManager View.
        View& find(vsg::View*);

        //! Adds a manipulator to a view.
        //! @param manip Manipulator to set
        //! @view View on which to set the manipulator
        void setManipulatorForView(vsg::ref_ptr<MapManipulator> manip, vsg::View* view);

        //! Gets the view containing the window coordinates (e.g., mouse position relative to the window)
        //! @param window Window to query
        //! @param x X coordinate in window space
        //! @param y Y coordinate in window space
        //! @return View containing the coordinates, or nullptr.
        std::tuple<Window, View> windowAndViewAtCoords(float x, float y);

        //! Gets the vulkan device shared by all windows
        vsg::ref_ptr<vsg::Device> sharedDevice() const;

        //! Compile and hook up a render graph that you have manually installed
        //! on a command graph.
        void compileRenderGraph(vsg::ref_ptr<vsg::RenderGraph>, vsg::ref_ptr<vsg::Window>);

        //! Callback fires when the user called addWindow
        Callback<Window&> onAddWindow;
        Callback<const Window&> onRemoveWindow;
        Callback<Window&, View&> onAddView;
        Callback<const Window&, const View&> onRemoveView;

    public:

        VSGContext vsgcontext;

    protected:

        std::vector<Window> _windows;

        bool _apilayer = false;
        bool _debuglayer = false;
        bool _vsync = true;
        bool _debuglayerUnique = false;
        bool _debugCallbackInstalled = false;

        void configureTraits(vsg::WindowTraits* traits);
    };
}
