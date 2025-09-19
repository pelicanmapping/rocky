/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/GeoPoint.h>
#include <rocky/Math.h>
#include <rocky/Viewpoint.h>
#include <optional>
#include <vector>

namespace vsg
{
    class Window;
}

namespace ROCKY_NAMESPACE
{
    class MapNode;

    /**
     * Programmable event handler that lets you control a camera using input devices,
     * designed for use with a geospatial display (2D map or 3D globe).
     */
    class ROCKY_EXPORT MapManipulator : public vsg::Inherit<vsg::Visitor, MapManipulator>
    {
    public:
        //! Construct a new manipulator
        MapManipulator(
            vsg::ref_ptr<MapNode> mapNode,
            vsg::ref_ptr<vsg::Window> window,
            vsg::ref_ptr<vsg::Camera> camera,
            VSGContext& context);

        virtual ~MapManipulator();

        //! Go to the home position.
        virtual void home();

        //! Move the focal point of the camera using deltas (normalized screen coords).
        virtual void pan(double dx, double dy);

        //! Rotate the camera (dx = azimuth, dy = pitch) using deltas (radians).
        virtual void rotate(double dx, double dy);

        //! Zoom the camera using deltas (dy only)
        virtual void zoom(double dx, double dy);

        //! Converts view coordinates (relative to the view's viewpoint) to world
        //! coordinates. Note, this method will use the mask set by setTraversalMask().
        //! @param x,y Viewport coordinates
        //! @param out_coords Output world coordinates (only valid if the method returns true)
        bool viewportToWorld(float x, float y, vsg::dvec3& out_coords) const;

        //! Distance from the focal point in world coordiantes
        double distance() const { return _state.distance; }

        //! Distance from the focal point in world coordinates.
        void setDistance(double distance);

        //! Sets the viewpoint immediately
        void setViewpoint(const Viewpoint& vp);

        //! Sets the viewpoint with optional transition time
        void setViewpoint(const Viewpoint& vp, std::chrono::duration<float> duration_s);

        //! Fetches the current viewpoint
        Viewpoint viewpoint() const;

        //! Clears the current viewpoint (if tethered or transitioning)
        void clearViewpoint();

        //! True if the user set a Viewpoint with a tethering target
        inline bool isTethering() const {
            return _state.setVP1.has_value() && _state.setVP1->pointFunction;
        }

        //! Store a reference to this manipulator in another object
        void put(vsg::ref_ptr<vsg::Object> object);

        //! Retrieve a reference to a mapmanipulator from and object
        static vsg::ref_ptr<MapManipulator> get(vsg::ref_ptr<vsg::Object> object);

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
            ActionOption() = default;
            ActionOption(int o, bool value) : option(o), boolValue(value) { }
            ActionOption(int o, int value) : option(o), intValue(value) { }
            ActionOption(int o, double value) : option(o), doubleValue(value) { }
            int option = 0;
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

        /**
        * Values and bindings that control the behavior of the manipulator.
        */
        class ROCKY_EXPORT Settings
        {
        public:
            // construct with default settings
            Settings() = default;

            // copy ctor
            Settings(const Settings& rhs) = default;

            //! Mouse sensitivity factor. Higher -> more sensitive.
            double mouseSensitivity = 1.0;

            //! Touch sensitivity factor. Higher -> more sensitive.
            double touchSensitivity = 0.005;

            //! Keyboard action sensitivity factor. This applies to navigation actions
            //! that are bound to keyboard events. For example, you may bind the LEFT arrow to
            //! the ACTION_PAN_LEFT action; this factor adjusts how much panning will occur during
            //! each frame that the key is depressed. Higher -> more sensitive.
            double keyboardSesitivity = 1.0;

            //! Scroll-wheel sensitivity factor. This applies to navigation actions
            //! that are bound to scrolling events. For example, you may bind the scroll wheel to
            //! the ACTION_ZOOM_IN action; this factor adjusts how much zooming will occur each time
            //! you click the scroll wheel. Higher = more sensitive.
            double scrollSensitivity = 1.0;

            //! Prevents simultaneous control of pitch and azimuth (when true).
            bool singleAxisRotation = false;

            //! Whether to lock in a camera heading when performing panning operations.
            bool lockAzimuthWhilePanning = true;

            //! Minimum allowable camera pitch relative to the planet (degrees)
            double minPitch = -89.99;

            //! Maximum allowable camera pitch relative to the planet (degrees)
            double maxPitch = -1.0;

            //! Minimum allowable distance from the focal point (meters)
            double minDistance = 1.0;

            //! Maximum allowable distance from the focal point (meters)
            double maxDistance = DBL_MAX;

            //! Whether a setViewpoint transition should "arc" over the planet surface, versus
            //! travel with a linearly-interpolated altitude
            bool arcViewpoints = true;

            //! Whether to automatically calculate a duration for calls to setViewpoint
            bool autoVPDuration = false;

            //! Minimum duration time when autoVPDuration = true (seconds)
            double minVPDuration = 3.0;

            //! Maximum duration time when autoVPDuration = true (seconds)
            double maxVPDuration = 8.0;

            //! Whtehr to zoom towards the mouse cursor when zooming
            bool zoomToMouse = true;


            //! Assigns behavior to the action of dragging the mouse while depressing one or
            //! more mouse buttons and modifier keys.
            //!
            //! @param action ActionType value to which to bind this mouse input specification
            //! @param button_mask Mask of MouseButtonMask values
            //! @param modkey_mask (default = 0) Mask of ModKeyMask values defining a modifier keycombination to associate with the action.
            //! @param options Action options (OPTION_CONTINUOUS, OPTION_SCALE_X, OPTION_SCALE_Y)
            void bindMouse(ActionType action, int button_mask, int modkey_mask = 0, const ActionOptions& options = {});

            //! Assigns a bevahior to the action of clicking one or more mouse buttons.
            //! 
            //! @param action Actiontype value to which to bind this input specification.
            //! @param button_mask Mask of MouseButtonMask values
            //! @param modkey_mask (default = 0L) A mask of ModKeyMask values defining a modifier key combination to associate with the action.
            //! @param options Action options (OPTION_GOTO_RANGE_FACTOR, OPTION_DURATION
            void bindMouseClick(ActionType action, int button_mask, int modkey_mask = 0L, const ActionOptions& options = {});

            //! Assigns a bevahior to the action of double-clicking one or more mouse buttons.
            //! 
            //!  @param action ActionType value to which to bind this double-click input specification.
            //!  @param button_mask Mask of MouseButtonMask values
            //!  @param modkey_mask Mask of ModKeyMask values defining a modifier key combination to associate with the action
            //!  @param options Action options (OPTION_GOTO_RANGE_FACTOR, OPTION_DURATION)
            //! 
            void bindMouseDoubleClick(ActionType action, int button_mask, int modkey_mask = 0L, const ActionOptions& options = {});

            //!  Assigns a bevahior to the action of depressing a key.
            //! 
            //!  @param action ActionType value to which to bind this key input specification.
            //!  @param key Key value
            //!  @param modkey_mask (default = 0L) Mask of ModKeyMask values defining a modifier keycombination to associate with the action.
            //!  @param options Action options (OPTION_CONTINUOUS)
            void bindKey(ActionType action, int key, int modkey_mask = 0, const ActionOptions& options = {});

            //! Assigns a bevahior to operation of the mouse's scroll wheel.
            //!
            //! @param action ActionType to which to bind this input spec
            //! @param scrolling_motion ScrollingMotion value
            //! @param modkey_mask Mask of ModKeyMask values defining a modifier key combination to associate with the action.
            //! @param options Action options (OPTION_SCALE_Y, OPTION_DURATION)
            void bindScroll(ActionType action, int scrolling_motion, int modkey_mask = 0L, const ActionOptions& options = {});

            //! TODO
            void bindPinch(ActionType action, const ActionOptions& = {});

            //! TODO
            void bindTwist(ActionType action, const ActionOptions& = {});

            //! TODO
            void bindMultiDrag(ActionType action, const ActionOptions& = {});

        private: // settings that we might port in the future

            //! Mode used for tethering to a node
            TetherMode tetherMode = TETHER_CENTER;

            //! Collection of Actions that will automatically break a tether.
            ActionTypeVector breakTetherActions;

            //! Max x offset in meters
            //double maxXOffset = 0.0;

            //! Max y offset in meters
            //double maxYOffset = 0.0;

            //! Whether to automatically adjust an orthographic camera so that it "tracks" the last known FOV and Aspect Ratio.
            //bool orthoTracksPerspective = true;

            //! Whether or not to keep the camera from going through the terrain surface
            //bool terrainAvoidance = false;

            //! Minimum range for terrain avoidance checks (meters)
            //double minTerrainAvoidanceDistance = 1.0;

            //! Whether a flick of the mouse or touch gesture will cause the camera to "throw" and continue moving after the gesture ends
            //bool throwing = false;

            //! Rate at which a throw will decay (0.0 = no decay, 1.0 = instant stop) when throwing == true
            //double throwDecayRate = 0.05;

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
        };

        Settings settings;

        //! Call if you replace the Settings object outright
        void dirty();

    protected:

        bool intersect(const vsg::dvec3& start, const vsg::dvec3& end, vsg::dvec3& intersection) const;

        bool intersectAlongLookVector(vsg::dvec3& out_world) const;

        // returns the absolute Euler angles composited from the composite rotation matrix.
        void compositeEulerAngles(double* out_azim, double* out_pitch = 0L) const;

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
                _type = type; _delta = delta; _duration_s = duration; _frameCount = 0;
            }
            void reset() {
                _type = TASK_NONE;
            }
            TaskType _type;
            vsg::dvec2 _delta;
            double _duration_s = DBL_MAX;
            int _frameCount = 0;
            //vsg::time_point _previousTick;
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

        bool recalculateCenterAndDistanceFromLookVector();

        vsg::dmat4 getWorldLookAtMatrix(const vsg::dvec3& center) const;

        bool isMouseClick(vsg::ButtonReleaseEvent&) const;

        void applyOptionsToDeltas(const Action& action, vsg::dvec2& delta);

        void configureDefaultSettings();

        void reinitialize();

        // sets the new center (focal) point and recalculates it's L2W matrix.
        void setCenter(const vsg::dvec3& center);

        // creates a "local-to-world" transform relative to the input point.
        bool createLocalCoordFrame(const vsg::dvec3& worldPos, vsg::dmat4& out_frame) const;

        // Applies an action using the raw input parameters.
        bool handleAction(const Action& action, const vsg::dvec2& delta, vsg::time_point now, double duration);

        virtual bool handleMouseAction(const Action& action, const vsg::MoveEvent& previous, const vsg::MoveEvent& current);
        virtual bool handleMouseClickAction(const Action& action, vsg::time_point time);
        virtual bool handleKeyboardAction(const Action& action, vsg::time_point time, double duration_s = DBL_MAX);
        virtual bool handleScrollAction(const Action& action, vsg::time_point time, double duration_s = DBL_MAX);
        virtual bool handlePointAction(const Action& type, float mx, float my, vsg::time_point time);
        virtual void handleMovementAction(const ActionType& type, vsg::dvec2 delta);

        void clearEvents();
        vsg::ref_ptr<MapNode> getMapNode() const;

        struct State
        {
            // The world coordinate of the focal point.
            vsg::dvec3 center;

            // Reference frame for the local ENU tangent plane to the elllipoid centered
            // at "_center" with (X=east, Y=north, Z=up), with the translation removed.
            vsg::dmat4 centerRotation = vsg::dmat4(1.0);

            // Quaternion that applies a heading and pitch in the local tangent plane
            // established by center and centerRotation.
            vsg::dquat localRotation = vsg::dquat( 0, 0, 0, 1 );

            // distance from camera to _center.
            double distance = 1.0;

            // XYZ offsets of the focal point in the local tangent plane coordinate system
            // of the focal point.
            vsg::dvec3 localPositionOffset;

            // XY offsets (left/right, down/up) of the focal point in the plane normal to
            // the view heading.
            vsg::dvec2 viewOffset;

            vsg::dquat tetherRotation = vsg::dquat(0, 0, 0, 1);

            std::optional<Viewpoint> setVP0;
            std::optional<Viewpoint> setVP1; // Final viewpoint
            option<vsg::time_point> setVPStartTime; // Time of call to setViewpoint
            std::chrono::duration<float> setVPDuration; // Transition time for setViewpoint
            double setVPAccel, setVPAccel2; // Acceleration factors for setViewpoint
            double setVPArcHeight; // Altitude arcing height for setViewpoint
            vsg::dquat tetherRotationVP0;
            vsg::dquat tetherRotationVP1;
            TetherMode lastTetherMode = TETHER_CENTER;
        };

        VSGContext _context;

        vsg::observer_ptr<MapNode> _mapNode_weakptr;
        vsg::observer_ptr<vsg::Window> _window_weakptr;
        vsg::observer_ptr<vsg::Camera> _camera_weakptr;

        option<vsg::MoveEvent> _previousMove;
        option<vsg::ButtonPressEvent> _buttonPress;
        option<vsg::KeyPressEvent> _keyPress;
        vsg::time_point _previousTime;

        bool _thrown;
        vsg::dvec2 _throwDelta;
        vsg::dvec2 _delta;
        vsg::dmat4 _viewMatrix;
        State _state;
        Task _task;
        int _continuous = 0;
        vsg::dvec2 _continuousDelta;
        vsg::dvec2 _singleAxis;
        Action _lastAction;
        Action _continuousAction;

        // rendering required b/c something changed.
        bool _dirty;

        bool withinRenderArea(const vsg::PointerEvent& pointerEvent) const;

        vsg::dvec2 ndc(const vsg::PointerEvent&) const;

        inline bool isSettingViewpoint() const {
            return _state.setVP0.has_value() && _state.setVP1.has_value();
        }

        double setViewpointFrame(const vsg::time_point&);

        void updateTether(const vsg::time_point& t);

        //! returns true if the camera changed.
        bool updateCamera();
    };
}
