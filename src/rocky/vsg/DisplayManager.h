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


    /**
    * Encaputes a "View" in the Rocky DisplayManager.
    * A view is a combination of a VSG view and its parent render graph.
    * This object contains immutable members, which means you can copy it be reference
    * and still refer to the same logical View.
    */
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
        template<class T> const T* find() const {
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

        const DisplayManager* _display = nullptr;

        friend class Window;
        friend class DisplayManager;
        friend class Application;
    };


    /**
    * Collection of views in a Window, as managed by the DisplayManager.
    * As the name implies they are ordered from front to back, meaning the first view
    * in the container is the front-most view in the window (or at least, the most
    * recently created one).
    *
    * The reverse ordering makes range-based iteration easier since you usually want
    * to prioritize the "topmost" view.
    *
    * This container also prevents writing the container itself.
    */
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

        //! number of views in the container
        std::size_t size() const {
            return _container.size();
        }

        //! access the n'th view in the container
        View& operator [] (std::size_t index) {
            return _container[index];
        }

        //! access the n'th view in the container
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


    /**
    * Represents a top-level Window in the Rocky DisplayManager.
    * This wraps a VSG Window, its associated CommandGraph, and any Views that live within it.
    * This object contains ONLY immutable members, which means you can copy it by reference and it
    * will always still refer to the same logical window.
    */
    class ROCKY_EXPORT Window
    {
    public:
        //! Create an invalid window.
        Window() = default;

        //! Copy a window (which will refer to the same logical Window)
        Window(const Window& rhs) = default;

        //! Adds a new View to the window with the given camera and scene graph.
        View& addView(vsg::ref_ptr<vsg::Camera>, vsg::ref_ptr<vsg::Node>);

        //! Adds a new View using an pre-populated VSG view
        View& addView(vsg::ref_ptr<vsg::View>);

        //! Removes a view from ths Window
        void removeView(View&);

        //! Views managed by this Window
        ViewsFrontToBack& views() { return *_views; }
        const ViewsFrontToBack& views() const { return *_views; }
        
        //! The n'th View in the window (from back to front- view 0 is usually the main view)
        View& view(std::size_t index = 0) {
            return _views->operator[](_views->size() - 1 - index);
        }

        //! The n'th View in the window (from back to front - view 0 is usually the main view)
        const View& view(std::size_t index = 0) const {
            return _views->operator[](_views->size() - 1 - index);
        }

        //! Locate the View containing the given window coordinates (e.g., mouse position)
        View& viewAtCoords(float x, float y);

        //! Validity operator
        operator bool() const {
            return vsgWindow && commandGraph && _display;
        }

        //! Equality operator
        bool operator == (const Window& rhs) const {
            return vsgWindow == rhs.vsgWindow;
        }

        //! Underlying VSG Window instance
        vsg::ref_ptr<vsg::Window> vsgWindow;

        //! Underlying CommandGraph for this VSG window
        vsg::ref_ptr<vsg::CommandGraph> commandGraph;

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

        //! The n'th window
        Window& window(std::size_t index = 0);
        const Window& window(std::size_t index = 0) const;

        //! All the windows
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
        void removeWindow(Window& window);

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

        //! Callbacks for window operations
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
