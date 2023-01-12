/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Common.h>

#include <rocky/GeoPoint.h>
#include <rocky/Math.h>
#include <rocky/Viewpoint.h>

#include <vsg/core/Inherit.h>
#include <vsg/core/Visitor.h>
#include <vsg/app/Camera.h>
#include <vsg/ui/KeyEvent.h>
#include <vsg/ui/UIEvent.h>
#include <vsg/ui/PointerEvent.h>

namespace vsg
{
    class Window;
    class View;
}

namespace rocky
{
    class MapNode;

    /**
     * A programmable manipulator suitable for use with geospatial terrains
     */
    class ROCKY_VSG_EXPORT MapManipulator : public vsg::Inherit<vsg::Visitor, MapManipulator>
    {
    public:
        //! Construct a new manipulator
        MapManipulator(
            vsg::ref_ptr<MapNode> mapNode,
            vsg::ref_ptr<vsg::Camera> camera);

        virtual ~MapManipulator();

        //! Go to the home position.
        virtual void home();

        //! Move the focal point of the camera using deltas (normalized screen coords).
        virtual void pan(double dx, double dy);

        //! Rotate the camera (dx = azimuth, dy = pitch) using deltas (radians).
        virtual void rotate(double dx, double dy);

        //! Zoom the camera using deltas (dy only)
        virtual void zoom(double dx, double dy);

        //! Converts screen coordinates (relative to the view's viewpoint) to world
        //! coordinates. Note, this method will use the mask set by setTraversalMask().
        //! @param x,y Viewport coordinates
        //! @param out_coords Output world coordinates (only valid if the method returns true)
        bool screenToWorld(float x, float y, vsg::dvec3& out_coords) const;

        //! Distance from the focal point in world coordiantes
        double getDistance() const { return _state.distance; }

        //! Distance from the focal point in world coordinates.
        void setDistance(double distance);

    public: // vsg::Visitor
        void apply(vsg::KeyPressEvent& keyPress) override;
        void apply(vsg::KeyReleaseEvent&) override;
        void apply(vsg::ButtonPressEvent& buttonPress) override;
        void apply(vsg::ButtonReleaseEvent& buttonRelease) override;
        void apply(vsg::MoveEvent& moveEvent) override;
        void apply(vsg::ScrollWheelEvent& scrollWheel) override;
        void apply(vsg::TouchDownEvent& touchDown) override;
        void apply(vsg::TouchUpEvent& touchUp) override;
        void apply(vsg::TouchMoveEvent& touchMove) override;
        void apply(vsg::FrameEvent& frame) override;

    public:

        //! Bindable manipulator actions
        enum ActionType {
            ACTION_NULL,
            ACTION_HOME,
            ACTION_GOTO,
            ACTION_PAN,
            ACTION_PAN_LEFT,
            ACTION_PAN_RIGHT,
            ACTION_PAN_UP,
            ACTION_PAN_DOWN,
            ACTION_ROTATE,
            ACTION_ROTATE_LEFT,
            ACTION_ROTATE_RIGHT,
            ACTION_ROTATE_UP,
            ACTION_ROTATE_DOWN,
            ACTION_ZOOM,
            ACTION_ZOOM_IN,
            ACTION_ZOOM_OUT,
            ACTION_EARTH_DRAG
        };

        //! Vector of action types
        using ActionTypeVector = std::vector<ActionType>;

        //! Bindable event types
        enum EventType {
            EVENT_MOUSE_DOUBLE_CLICK,
            EVENT_MOUSE_DRAG,
            EVENT_KEY_DOWN,
            EVENT_SCROLL,
            EVENT_MOUSE_CLICK,
            EVENT_MULTI_DRAG,
            EVENT_MULTI_PINCH,
            EVENT_MULTI_TWIST
        };

        //! Bindable mouse buttons
        enum MouseEvent {
            MOUSE_LEFT_BUTTON = vsg::ButtonMask::BUTTON_MASK_1,
            MOUSE_MIDDLE_BUTTON = vsg::ButtonMask::BUTTON_MASK_2,
            MOUSE_RIGHT_BUTTON = vsg::ButtonMask::BUTTON_MASK_3
        };

        /** Action options. Certain options are only meaningful to certain Actions.
            See the bind* documentation for information. */
        enum ActionOptionType {
            OPTION_SCALE_X,             // Sensitivity multiplier for horizontal input movements
            OPTION_SCALE_Y,             // Sensitivity multiplier for vertical input movements
            OPTION_CONTINUOUS,          // Whether to act as long as the button or key is depressed
            OPTION_SINGLE_AXIS,         // If true, only operate on one axis at a time (the one with the larger value)
            OPTION_GOTO_RANGE_FACTOR,   // for ACTION_GOTO, multiply the Range by this factor (to zoom in/out)
            OPTION_DURATION             // Time it takes to complete the action (in seconds)
        };

        //! Tethering options
        enum TetherMode
        {
            TETHER_CENTER,              // The camera will follow the center of the node.
            TETHER_CENTER_AND_ROTATION, // The camera will follow the node and all rotations made by the node
            TETHER_CENTER_AND_HEADING   // The camera will follow the node and only follow heading rotation
        };

        //! Camera projection matrix type
        enum CameraProjection
        {
            PROJ_PERSPECTIVE,
            PROJ_ORTHOGRAPHIC
        };

        struct ActionOption {
            ActionOption() { }
            ActionOption(int o, bool value) : option(o), boolValue(value) { }
            ActionOption(int o, int value) : option(o), intValue(value) { }
            ActionOption(int o, double value) : option(o), doubleValue(value) { }
            int option;
            union {
                bool boolValue;
                int intValue;
                double doubleValue;
            };
        };

        struct ActionOptions : public std::vector<ActionOption> {
            void add(int option, bool value) { push_back(ActionOption(option, value)); }
            void add(int option, int value) { push_back(ActionOption(option, value)); }
            void add(int option, double value) { push_back(ActionOption(option, value)); }
        };

    protected:
        struct InputSpec
        {
            InputSpec(int event_type, int input_mask, int modkey_mask);
            InputSpec(const InputSpec& rhs);
            bool operator == (const InputSpec& rhs) const;
            bool operator < (const InputSpec& rhs) const;
            int _event_type;
            int _input_mask;
            int _modkey_mask;
        };
        using InputSpecs = std::list<InputSpec>;

        enum Direction {
            DIR_NA,
            DIR_LEFT,
            DIR_RIGHT,
            DIR_UP,
            DIR_DOWN
        };

        struct Action {
            Action(ActionType type = ACTION_NULL);
            Action(ActionType type, const ActionOptions& options);
            Action(const Action& rhs);
            ActionType _type;
            Direction _dir;
            ActionOptions _options;
            bool getBoolOption(int option, bool defaultValue) const;
            int getIntOption(int option, int defaultValue) const;
            double getDoubleOption(int option, double defaultValue) const;
        private:
            void init();
        };

        static Action NullAction;

    public:

        class ROCKY_VSG_EXPORT Settings
        {
        public:
            // construct with default settings
            Settings();

            // copy ctor
            Settings(const Settings& rhs);

            /** dtor */
            virtual ~Settings() { }

            void dirty() { } 

            /**
             * Assigns behavior to the action of dragging the mouse while depressing one or
             * more mouse buttons and modifier keys.
             *
             * @param action
             *      The EarthManipulator::ActionType value to which to bind this mouse
             *      input specification.
             *
             * @param button_mask
             *      Mask of osgGA::GUIEventAdapter::MouseButtonMask values
             *
             * @param modkey_mask (default = 0L)
             *      A mask of osgGA::GUIEventAdapter::ModKeyMask values defining a modifier key
             *      combination to associate with the action.
             *
             * @param options
             *      Action options. Valid options are:
             *      OPTION_CONTINUOUS, OPTION_SCALE_X, OPTION_SCALE_Y
             */
            void bindMouse(
                ActionType action,
                int button_mask,
                int modkey_mask = 0L,
                const ActionOptions& options = ActionOptions());

            /**
             * Assigns a bevahior to the action of clicking one or more mouse buttons.
             *
             * @param action
             *      The EarthManipulator::ActionType value to which to bind this mouse click
             *      input specification.
             *
             * @param button_mask
             *      Mask of osgGA::GUIEventAdapter::MouseButtonMask values
             *
             * @param modkey_mask (default = 0L)
             *      A mask of osgGA::GUIEventAdapter::ModKeyMask values defining a modifier key
             *      combination to associate with the action.
             *
             * @param options
             *      Action options. Valid options are:
             *      OPTION_GOTO_RANGE_FACTOR, OPTION_DURATION
             */
            void bindMouseClick(
                ActionType action,
                int button_mask,
                int modkey_mask = 0L,
                const ActionOptions& options = ActionOptions());

            /**
             * Assigns a bevahior to the action of double-clicking one or more mouse buttons.
             *
             * @param action
             *      The EarthManipulator::ActionType value to which to bind this double-click
             *      input specification.
             *
             * @param button_mask
             *      Mask of osgGA::GUIEventAdapter::MouseButtonMask values
             *
             * @param modkey_mask (default = 0L)
             *      A mask of osgGA::GUIEventAdapter::ModKeyMask values defining a modifier key
             *      combination to associate with the action.
             *
             * @param options
             *      Action options. Valid options are:
             *      OPTION_GOTO_RANGE_FACTOR, OPTION_DURATION
             */
            void bindMouseDoubleClick(
                ActionType action,
                int button_mask,
                int modkey_mask = 0L,
                const ActionOptions& options = ActionOptions());

            /**
             * Assigns a bevahior to the action of depressing a key.
             *
             * @param action
             *      The EarthManipulator::ActionType value to which to bind this key
             *      input specification.
             *
             * @param key
             *      A osgGA::GUIEventAdapter::KeySymbol value
             *
             * @param modkey_mask (default = 0L)
             *      A mask of osgGA::GUIEventAdapter::ModKeyMask values defining a modifier key
             *      combination to associate with the action.
             *
             * @param options
             *      Action options. Valid options are:
             *      OPTION_CONTINUOUS
             */
            void bindKey(
                ActionType action,
                int key,
                int modkey_mask = 0L,
                const ActionOptions& options = ActionOptions());

            /**
             * Assigns a bevahior to operation of the mouse's scroll wheel.
             *
             * @param action
             *      The EarthManipulator::ActionType value to which to bind this scroll
             *      input specification.
             *
             * @param scrolling_motion
             *      A osgGA::GUIEventAdapter::ScrollingMotion value
             *
             * @param modkey_mask (default = 0L)
             *      A mask of osgGA::GUIEventAdapter::ModKeyMask values defining a modifier key
             *      combination to associate with the action.
             *
             * @param options
             *      Action options. Valid options are:
             *      OPTION_SCALE_Y, OPTION_DURATION
             */
            void bindScroll(
                ActionType action,
                int scrolling_motion,
                int modkey_mask = 0L,
                const ActionOptions& options = ActionOptions());


            void bindPinch(
                ActionType action, const ActionOptions & = ActionOptions());

            void bindTwist(
                ActionType action, const ActionOptions & = ActionOptions());

            void bindMultiDrag(
                ActionType action, const ActionOptions & = ActionOptions());

            /**
             * Sets an overall mouse sensitivity factor.
             *
             * @param value
             *      A scale factor to apply to mouse readings.
             *      1.0 = default; < 1.0 = less sensitive; > 1.0 = more sensitive.
             */
            void setMouseSensitivity(double value) { _mouse_sens = value; }

            /**
             * Gets the overall mouse sensitivity scale factor. Default = 1.0.
             */
            double getMouseSensitivity() const { return _mouse_sens; }

            /**
             * Sets an overall touch sensitivity factor.
             *
             * @param value
             *      A scale factor to apply to mouse readings.
             *      0.005 = default; < 0.005 = less sensitive; > 0.005 = more sensitive.
             */
            void setTouchSensitivity(double value) { _touch_sens = value; }

            /**
             * Gets the overall touch sensitivity scale factor. Default = 1.0.
             */
            double getTouchSensitivity() const { return _touch_sens; }

            /**
             * Sets the keyboard action sensitivity factor. This applies to navigation actions
             * that are bound to keyboard events. For example, you may bind the LEFT arrow to
             * the ACTION_PAN_LEFT action; this factor adjusts how much panning will occur during
             * each frame that the key is depressed.
             *
             * @param value
             *      A scale factor to apply to keyboard-controller navigation.
             *      1.0 = default; < 1.0 = less sensitive; > 1.0 = more sensitive.
             */
            void setKeyboardSensitivity(double value) { _keyboard_sens = value; }

            /**
             * Gets the keyboard action sensitivity scale factor. Default = 1.0.
             */
            double getKeyboardSensitivity() const { return _keyboard_sens; }

            /**
             * Sets the scroll-wheel sensitivity factor. This applies to navigation actions
             * that are bound to scrolling events. For example, you may bind the scroll wheel to
             * the ACTION_ZOOM_IN action; this factor adjusts how much zooming will occur each time
             * you click the scroll wheel.
             *
             * @param value
             *      A scale factor to apply to scroll-wheel-controlled navigation.
             *      1.0 = default; < 1.0 = less sensitive; > 1.0 = more sensitive.
             */
            void setScrollSensitivity(double value) { _scroll_sens = value; }

            /**
             * Gets the scroll wheel sensetivity scale factor. Default = 1.0.
             */
            double getScrollSensitivity() const { return _scroll_sens; }

            /**
             * When set to true, prevents simultaneous control of pitch and azimuth.
             *
             * Usually you can alter pitch and azimuth at the same time. When this flag
             * is set, you can only control one at a time - if you start slewing the azimuth of the camera,
             * the pitch stays locked until you stop moving and then start slewing the pitch.
             *
             * Default = false.
             */
            void setSingleAxisRotation(bool value) { _single_axis_rotation = value; }

            /**
             * Gets whether simultaneous control over pitch and azimuth is disabled.
             * Default = false.
             */
            bool getSingleAxisRotation() const { return _single_axis_rotation; }

            /**
             * Sets whether to lock in a camera heading when performing panning operations (i.e.,
             * changing the focal point).
             */
            void setLockAzimuthWhilePanning(bool value) { _lock_azim_while_panning = value; }

            /**
             * Gets true if the manipulator should lock in a camera heading when performing panning
             * operations (i.e. changing the focal point.)
             */
            bool getLockAzimuthWhilePanning() const { return _lock_azim_while_panning; }

            /**
             * Sets the minimum and maximum allowable local camera pitch, in degrees.
             *
             * By "local" we mean relative to the tangent plane passing through the focal point on
             * the surface of the terrain.
             *
             * Defaults are: Min = -90, Max = -10.
             */
            void setMinMaxPitch(double min_pitch, double max_pitch);

            /** Gets the minimum allowable local pitch, in degrees. */
            double getMinPitch() const { return _min_pitch; }

            /** Gets the maximum allowable local pitch, in degrees. */
            double getMaxPitch() const { return _max_pitch; }

            /** Gets the max x offset in world coordinates */
            double getMaxXOffset() const { return _max_x_offset; }

            /** Gets the max y offset in world coordinates */
            double getMaxYOffset() const { return _max_y_offset; }

            /** Gets the minimum distance from the focal point in world coordinates */
            double getMinDistance() const { return _min_distance; }

            /** Gets the maximum distance from the focal point in world coordinates */
            double getMaxDistance() const { return _max_distance; }

            /** Sets the min and max distance from the focal point in world coordinates */
            void setMinMaxDistance(double min_distance, double max_distance);

            /**
            * Sets the maximum allowable offsets for the x and y camera offsets in world coordinates
            */
            void setMaxOffset(double max_x_offset, double max_y_offset);

            /** Mode used for tethering to a node. */
            void setTetherMode(TetherMode value) { _tether_mode = value; }
            TetherMode getTetherMode() const { return _tether_mode; }

            /** Access to the list of Actions that will automatically break a tether */
            ActionTypeVector& getBreakTetherActions() { return _breakTetherActions; }
            const ActionTypeVector& getBreakTetherActions() const { return _breakTetherActions; }

            /** Whether a setViewpoint transition whould "arc" */
            void setArcViewpointTransitions(bool value);
            bool getArcViewpointTransitions() const { return _arc_viewpoints; }

            /** Activates auto-duration for transitioned viewpoints. */
            void setAutoViewpointDurationEnabled(bool value);
            bool getAutoViewpointDurationEnabled() const { return _auto_vp_duration; }

            void setAutoViewpointDurationLimits(double minSeconds, double maxSeconds);
            void getAutoViewpointDurationLimits(double& out_minSeconds, double& out_maxSeconds) const {
                out_minSeconds = _min_vp_duration_s;
                out_maxSeconds = _max_vp_duration_s;
            }

            /** Whether to automatically adjust an orthographic camera so that it "tracks" the last known FOV and Aspect Ratio. */
            bool getOrthoTracksPerspective() const { return _orthoTracksPerspective; }
            void setOrthoTracksPerspective(bool value) { _orthoTracksPerspective = value; }

            /** Whether or not to keep the camera from going through the terrain surface */
            bool getTerrainAvoidanceEnabled() const { return _terrainAvoidanceEnabled; }
            void setTerrainAvoidanceEnabled(bool value) { _terrainAvoidanceEnabled = value; }

            /** Minimum range for terrain avoidance checks in world coordinates */
            double getTerrainAvoidanceMinimumDistance() const { return _terrainAvoidanceMinDistance; }
            void setTerrainAvoidanceMinimumDistance(double minDistance) { _terrainAvoidanceMinDistance = minDistance; }

            void setThrowingEnabled(bool throwingEnabled) { _throwingEnabled = throwingEnabled; }
            bool getThrowingEnabled() const { return _throwingEnabled; }

            void setThrowDecayRate(double throwDecayRate) { _throwDecayRate = clamp(throwDecayRate, 0.0, 1.0); }
            double getThrowDecayRate() const { return _throwDecayRate; }

            void setZoomToMouse(bool value) { _zoomToMouse = value; }
            bool getZoomToMouse() const { return _zoomToMouse; }

        private:

            friend class MapManipulator;

            typedef std::pair<InputSpec, Action> ActionBinding;
            typedef std::map<InputSpec, Action> ActionBindings;

            // Gets the action bound to the provided input specification, or NullAction if there is
            // to matching binding.
            const Action& getAction(int event_type, int input_mask, int modkey_mask) const;

            void expandSpec(const InputSpec& input, InputSpecs& output) const;
            void bind(const InputSpec& spec, const Action& action);

        private:

            ActionBindings _bindings;
            bool _single_axis_rotation;
            bool _lock_azim_while_panning;
            double _mouse_sens;
            double _keyboard_sens;
            double _scroll_sens;
            double _touch_sens;
            double _min_pitch;
            double _max_pitch;

            double _max_x_offset;
            double _max_y_offset;

            double _min_distance;
            double _max_distance;

            TetherMode _tether_mode;
            ActionTypeVector _breakTetherActions;
            bool _arc_viewpoints;
            bool _auto_vp_duration;
            double _min_vp_duration_s, _max_vp_duration_s;

            bool _orthoTracksPerspective;

            bool _terrainAvoidanceEnabled;
            double _terrainAvoidanceMinDistance;

            bool _throwingEnabled;
            double _throwDecayRate;

            bool _zoomToMouse;
        };

        shared_ptr<Settings> _settings;

        void applySettings(shared_ptr<Settings>);

        shared_ptr<Settings> getSettings() const;

    protected:

        bool intersect(const vsg::dvec3& start, const vsg::dvec3& end, vsg::dvec3& intersection) const;

        bool intersectAlongLookVector(vsg::dvec3& out_world) const;

        // returns the absolute Euler angles composited from the composite rotation matrix.
        void getCompositeEulerAngles(double* out_azim, double* out_pitch = 0L) const;

        // This sets the camera's roll based on your location on the globe.
        void recalculateRoll();

        enum TaskType
        {
            TASK_NONE,
            TASK_PAN,
            TASK_ROTATE,
            TASK_ZOOM
        };

        struct Task
        {
            Task() : _type(TASK_NONE) { }
            void set(TaskType type, const vsg::dvec2& delta, double duration, vsg::time_point now) {
                _type = type; _delta = delta; _duration_s = duration; _time_last_service = now;
            }
            void reset() {
                _type = TASK_NONE;
            }
            TaskType _type;
            vsg::dvec2 _delta;
            double _duration_s;
            vsg::time_point  _time_last_service;
        };

        // "ticks" the resident Task, which allows for multi-frame animation of navigation
        // movements.
        bool serviceTask(vsg::time_point);

        // returns the Euler Angles baked into _rotation, the local frame's rotation quaternion.
        void getEulerAngles(const vsg::dquat& quat, double* azim, double* pitch) const;

        // Makes a quaternion from an azimuth and pitch.
        vsg::dquat getQuaternion(double azim, double pitch) const;

        //! Fire a ray from the current eyepoint along the current look vector,
        //! intersect the terrain at the closest point, and make that the new focal point.
        bool recalculateCenterFromLookVector();

        vsg::dmat4 getWorldLookAtMatrix(const vsg::dvec3& center) const;

        bool isMouseClick() const;

        void applyOptionsToDeltas(const Action& action, vsg::dvec2& delta);

        void configureDefaultSettings();

        void reinitialize();

        // sets the new center (focal) point and recalculates it's L2W matrix.
        void setCenter(const vsg::dvec3& center);

        // creates a "local-to-world" transform relative to the input point.
        bool createLocalCoordFrame(const vsg::dvec3& worldPos, vsg::dmat4& out_frame) const;

        // returns an ActionType that would be initiated by the OSG UI event
        ActionType getActionTypeForEvent(const vsg::WindowEvent& ea) const;

        // Applies an action using the raw input parameters.
        bool handleAction(const Action& action, const vsg::dvec2& delta, vsg::time_point now, double duration);

        virtual bool handleMouseAction(const Action& action, vsg::time_point time);
        virtual bool handleMouseClickAction(const Action& action, vsg::time_point time);
        virtual bool handleKeyboardAction(const Action& action, vsg::time_point time, double duration_s = DBL_MAX);
        virtual bool handleScrollAction(const Action& action, vsg::time_point time, double duration_s = DBL_MAX);
        virtual bool handlePointAction(const Action& type, float mx, float my, vsg::time_point time);
        virtual void handleMovementAction(const ActionType& type, vsg::dvec2 delta, vsg::time_point time);
        virtual void handleContinuousAction(const Action& action, vsg::time_point time);

        void clearEvents();
        vsg::ref_ptr<MapNode> getMapNode() const;

        struct State
        {
            // The world coordinate of the Viewpoint focal point.
            vsg::dvec3 center;

            // Reference frame for the local ENU tangent plane to the elllipoid centered
            // at "_center" with (X=east, Y=north, Z=up)
            vsg::dmat4 centerRotation;

            // Quaternion that applies a heading and pitch in the local tangent plane
            // established by _center and _centerRotation.
            vsg::dquat localRotation;

            // distance from camera to _center.
            double distance;

            // XYZ offsets of the focal point in the local tangent plane coordinate system
            // of the focal point.
            vsg::dvec3 localPositionOffset;

            // XY offsets (left/right, down/up) of the focal point in the plane normal to
            // the view heading.
            vsg::dvec2 viewOffset;

            State() :
                localRotation(0, 0, 0, 1),
                distance(1.0)
            { }
        };

        vsg::observer_ptr<MapNode> _mapNode;
        vsg::ref_ptr<vsg::Camera> _camera;
        SRS _worldSRS;

        optional<vsg::MoveEvent> _currentMove, _previousMove;
        optional<vsg::ButtonPressEvent> _buttonPress;
        optional<vsg::ButtonReleaseEvent> _buttonRelease;
        optional<vsg::KeyPressEvent> _keyPress;

        bool _thrown;
        vsg::dvec2 _throwDelta;
        vsg::dvec2 _delta;
        vsg::dmat4 _viewMatrix;
        State _state;
        Task _task;
        bool _continuous;
        vsg::dvec2 _continuousDelta;
        vsg::time_point _last_continuous_action_time;
        vsg::dvec2 _singleAxis;
        vsg::dmat4 _mapNodeFrame, _mapNodeFrameInverse;
        Action _lastAction;
        Action _continuousAction;

        // rendering required b/c something changed.
        bool _dirty;

        /// list of windows and theire xy offsets
        std::map<vsg::observer_ptr<vsg::Window>, vsg::ivec2> _windowOffsets;

        /// add a Window to respond events for, with mouse coordinate offset to treat all associated windows
        void addWindow(vsg::ref_ptr<vsg::Window> window, const vsg::ivec2& offset = {});

        //void collisionDetect();

        std::pair<int32_t, int32_t> cameraRenderAreaCoordinates(const vsg::PointerEvent&) const;

        bool withinRenderArea(const vsg::PointerEvent& pointerEvent) const;

        vsg::dvec2 ndc(const vsg::PointerEvent&) const;
    };

}
