/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MapManipulator.h"
#include "MapNode.h"
#include "VSGUtils.h"
#include <rocky/Units.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#undef LC
#define LC "[MapManipulator] "

#define TEST_OUT if(true) Log::info()
#define NOT_YET_IMPLEMENTED(X) ROCKY_TODO(X)

namespace
{
    // a reasonable approximation of cosine interpolation
    double
    smoothStepInterp( double t ) {
        return (t*t)*(3.0-2.0*t);
    }

    // rough approximation of pow(x,y)
    double
    powFast( double x, double y ) {
        return x/(x+y-y*x);
    }

    // accel/decel curve (a < 0 => decel)
    double
    accelerationInterp( double t, double a ) {
        return a == 0.0? t : a > 0.0? powFast( t, a ) : 1.0 - powFast(1.0-t, -a);
    }

    // normalized linear intep
    template<class DVEC3>
    vsg::dvec3 nlerp(const DVEC3& a, const DVEC3& b, double t) {
        double am = length(a), bm = length(b);
        vsg::dvec3 c = a*(1.0-t) + b*t;
        c = normalize(c);
        c *= (1.0-t)*am + t*bm;
        return c;
    }

    // linear interp
    template<class DVEC3>
    vsg::dvec3 lerp(const DVEC3& a, const DVEC3& b, double t) {
        return a*(1.0-t) + b*t;
    }

    template<class DMAT4>
    inline vsg::dvec3 getTrans(const DMAT4& m) {
        return vsg::dvec3(m[3][0], m[3][1], m[3][2]);
    }

    template<class DMAT4>
    inline vsg::dvec3 getXAxis(const DMAT4& cf) {
        return vsg::dvec3(cf[0][0], cf[0][1], cf[0][2]);
    }

    template<class DMAT4>
    inline vsg::dvec3 getYAxis(const DMAT4& cf) {
        return vsg::dvec3(cf[1][0], cf[1][1], cf[1][2]);
    }

    template<class DMAT4>
    inline vsg::dvec3 getZAxis(const DMAT4& cf) {
        return vsg::dvec3(cf[2][0], cf[2][1], cf[2][2]);
    }

    template<class DURATION>
    double to_seconds(const DURATION& d) {
        auto temp = std::chrono::duration_cast<std::chrono::nanoseconds>(d);
        return 1e-9 * (double)temp.count();
    }

    template<class DMAT4>
    DMAT4 extract_rotation(const DMAT4& m) {
        DMAT4 r = m;
        r[3][0] = r[3][1] = r[3][2] = 0.0;
        return r;
    }

    glm::dvec3 computeWorld(vsg::ref_ptr<vsg::Node> node)
    {
        // TODO
        return glm::dvec3(0, 0, 0);
    }

    double normalizeAzimRad( double input )
    {
        if(fabs(input) > 2*M_PI)
            input = fmod(input,2*M_PI);
        if( input < -M_PI ) input += M_PI*2.0;
        if( input > M_PI ) input -= M_PI*2.0;
        return input;
    }
}

void
MapManipulator::put(vsg::ref_ptr<vsg::Object> object)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(object, void());
    object->setObject("rocky.mapmanipulator", vsg::ref_ptr<MapManipulator>(this));
}

vsg::ref_ptr<MapManipulator>
MapManipulator::get(vsg::ref_ptr<vsg::Object> object)
{   
    if (object)
        return object->getRefObject<MapManipulator>("rocky.mapmanipulator");
    else
        return { };
}


MapManipulator::Action::Action(ActionType type, const ActionOptions& options) :
    _type(type),
    _options(options)
{
    init();
}

MapManipulator::Action::Action(ActionType type) :
    _type(type)
{
    init();
}

void
MapManipulator::Action::init()
{
    _dir =
        _type == ACTION_PAN_LEFT || _type == ACTION_ROTATE_LEFT ? DIR_LEFT :
        _type == ACTION_PAN_RIGHT || _type == ACTION_ROTATE_RIGHT ? DIR_RIGHT :
        _type == ACTION_PAN_UP || _type == ACTION_ROTATE_UP || _type == ACTION_ZOOM_IN ? DIR_UP :
        _type == ACTION_PAN_DOWN || _type == ACTION_ROTATE_DOWN || _type == ACTION_ZOOM_OUT ? DIR_DOWN :
        DIR_NA;
}

MapManipulator::Action::Action(const Action& rhs) :
    _type(rhs._type),
    _dir(rhs._dir),
    _options(rhs._options)
{
    //nop
}

bool
MapManipulator::Action::getBoolOption(int option, bool defaultValue) const
{
    for (auto& i : _options)
        if (i.option == option)
            return i.boolValue;
    
    return defaultValue;
}

int
MapManipulator::Action::getIntOption(int option, int defaultValue) const
{
    for (auto& i : _options)
        if (i.option == option)
            return i.intValue;

    return defaultValue;
}

double
MapManipulator::Action::getDoubleOption(int option, double defaultValue) const
{
    for (auto& i : _options)
        if (i.option == option)
            return i.doubleValue;

    return defaultValue;
}

/****************************************************************************/

MapManipulator::Action MapManipulator::NullAction( MapManipulator::ACTION_NULL );

namespace
{
    static std::string s_actionNames[] = {
        "null",
        "home",
        "goto",
        "pan",
        "pan-left",
        "pan-right",
        "pan-up",
        "pan-down",
        "rotate",
        "rotate-left",
        "rotate-right",
        "rotate-up",
        "rotate-down",
        "zoom",
        "zoom-in",
        "zoom-out",
        "earth-drag"
    };

    static std::string s_actionOptionNames[] = {
        "scale-x",
        "scale-y",
        "continuous",
        "single-axis",
        "goto-range-factor",
        "duration"
    };

    static short s_actionOptionTypes[] = { 1, 1, 0, 0, 1, 1 }; // 0=bool, 1=double
}

//------------------------------------------------------------------------


MapManipulator::InputSpec::InputSpec(int event_type, int input_mask, int modkey_mask)
    : _event_type(event_type), _input_mask(input_mask), _modkey_mask(modkey_mask) { }

MapManipulator::InputSpec::InputSpec(const InputSpec& rhs)
    : _event_type(rhs._event_type), _input_mask(rhs._input_mask), _modkey_mask(rhs._modkey_mask) { }

bool
MapManipulator::InputSpec::operator == (const InputSpec& rhs) const {
    return
        _event_type == rhs._event_type &&
        _input_mask == rhs._input_mask &&
        ((_modkey_mask | vsg::MODKEY_NumLock) == (rhs._modkey_mask | vsg::MODKEY_NumLock));
}

bool
MapManipulator::InputSpec::operator < (const InputSpec& rhs) const {
    if (_event_type < rhs._event_type) return true;
    else if (_event_type > rhs._event_type) return false;
    else if (_input_mask < rhs._input_mask) return true;
    else if (_input_mask > rhs._input_mask) return false;
    else return (_modkey_mask < rhs._modkey_mask);
}

#define HASMODKEY( W, V ) (( W & V ) == V )

// expands one input spec into many if necessary, to deal with modifier key combos.
void
MapManipulator::Settings::expandSpec(const InputSpec& input, InputSpecs& output) const
{
    int e = input._event_type;
    int i = input._input_mask;
    int m = input._modkey_mask;

    if (HASMODKEY(m, vsg::MODKEY_Control))
    {
        //expandSpec(InputSpec(e, i, m & ~vsg::KEY_Control_L), output);
        //expandSpec(InputSpec(e, i, m & ~vsg::KEY_Control_R), output);
    }
    else if (HASMODKEY(m, vsg::MODKEY_Alt))
    {
        //expandSpec(InputSpec(e, i, m & ~vsg::KEY_Alt_L), output);
        //expandSpec(InputSpec(e, i, m & ~vsg::KEY_Alt_R), output);
    }
    else if (HASMODKEY(m, vsg::MODKEY_Shift))
    {
        //expandSpec(InputSpec(e, i, m & ~vsg::KEY_Shift_L), output);
        //expandSpec(InputSpec(e, i, m & ~vsg::KEY_Shift_R), output);
    }
    else if (HASMODKEY(m, vsg::MODKEY_Meta))
    {
        //expandSpec(InputSpec(e, i, m & ~vsg::KEY_Meta_L), output);
        //expandSpec(InputSpec(e, i, m & ~vsg::KEY_Meta_R), output);
    }

    //Always add the input so if we are dealing with a windowing system like QT that just sends MODKEY_CTRL it will still work.
    output.push_back(input);
}

void
MapManipulator::Settings::bind(const InputSpec& spec, const Action& action)
{
    InputSpecs specs;
    expandSpec(spec, specs);
    for (auto& spec : specs)
        _bindings[spec] = action;
}

void
MapManipulator::Settings::bindMouse(ActionType actionType, int button_mask, int modkey_mask, const ActionOptions& options)
{
    bind(InputSpec(EVENT_MOUSE_DRAG, button_mask, modkey_mask), Action(actionType, options));
}

void
MapManipulator::Settings::bindMouseClick(ActionType action, int button_mask, int modkey_mask, const ActionOptions& options)
{
    bind(InputSpec(EVENT_MOUSE_CLICK, button_mask, modkey_mask), Action(action, options));
}

void
MapManipulator::Settings::bindMouseDoubleClick(ActionType action, int button_mask, int modkey_mask, const ActionOptions& options)
{
    bind(InputSpec(EVENT_MOUSE_DOUBLE_CLICK, button_mask, modkey_mask), Action(action, options));
}

void
MapManipulator::Settings::bindKey(ActionType action, int key, int modkey_mask, const ActionOptions& options)
{
    bind(InputSpec(EVENT_KEY_DOWN, key, modkey_mask), Action(action, options));
}

void
MapManipulator::Settings::bindScroll(ActionType action, int scrolling_direction, int modkey_mask, const ActionOptions& options)
{
    bind(InputSpec(EVENT_SCROLL, scrolling_direction, modkey_mask), Action(action, options));
}

void
MapManipulator::Settings::bindPinch(ActionType action, const ActionOptions& options)
{
    bind(InputSpec(MapManipulator::EVENT_MULTI_PINCH, 0, 0), Action(action, options));
}

void
MapManipulator::Settings::bindTwist(ActionType action, const ActionOptions& options)
{
    bind(InputSpec(MapManipulator::EVENT_MULTI_TWIST, 0, 0), Action(action, options));
}

void
MapManipulator::Settings::bindMultiDrag(ActionType action, const ActionOptions& options)
{
    bind(InputSpec(MapManipulator::EVENT_MULTI_DRAG, 0, 0), Action(action, options));
}

const MapManipulator::Action&
MapManipulator::Settings::getAction(int event_type, int input_mask, int modkey_mask) const
{
    //Build the input spec but remove the numlock and caps lock from the modkey mask.  On Linux these seem to be passed in as part of the modkeymask
    //if they are on.  So if you bind an action like SCROLL to a modkey mask of 0 or a modkey mask of ctrl it will never match the spec exactly b/c
    //the modkey mask also includes capslock and numlock.
    InputSpec spec(event_type, input_mask, modkey_mask & ~vsg::MODKEY_NumLock & ~vsg::MODKEY_CapsLock);
    ActionBindings::const_iterator i = _bindings.find(spec);
    return i != _bindings.end() ? i->second : NullAction;
}


/************************************************************************/

MapManipulator::MapManipulator(
    vsg::ref_ptr<MapNode> mapNode,
    vsg::ref_ptr<vsg::Window> window,
    vsg::ref_ptr<vsg::Camera> camera,
    VSGContext& context) :

    Inherit(),
    _mapNode_weakptr(mapNode),
    _window_weakptr(window),
    _camera_weakptr(camera),
    _context(context),
    _lastAction(ACTION_NULL)
{
    if (mapNode.valid())
        _worldSRS = mapNode->srs();

    reinitialize();
    configureDefaultSettings();
    home();
}

MapManipulator::~MapManipulator()
{
    //nop
}

void
MapManipulator::configureDefaultSettings()
{
    _settings = std::make_shared<Settings>();

    // install default action bindings:
    ActionOptions options;

    _settings->bindKey(ACTION_HOME, vsg::KEY_Space);

    options.clear();
    options.add(OPTION_CONTINUOUS, true);

    // zoom as you hold the right button:
    _settings->bindMouse(ACTION_ZOOM, MOUSE_RIGHT_BUTTON, 0L, options);
    _settings->bindMouse(ACTION_ZOOM, MOUSE_RIGHT_BUTTON, vsg::MODKEY_Control, options);

    _settings->bindMouse(ACTION_PAN, MOUSE_LEFT_BUTTON, 0L);
    _settings->bindMouse(ACTION_PAN, MOUSE_LEFT_BUTTON, vsg::MODKEY_Control, options);

    // rotate with either the middle button or the left+right buttons:
    _settings->bindMouse(ACTION_ROTATE, MOUSE_MIDDLE_BUTTON, 0L);
    _settings->bindMouse(ACTION_ROTATE, MOUSE_LEFT_BUTTON | MOUSE_RIGHT_BUTTON, 0L);
    _settings->bindMouse(ACTION_ROTATE, MOUSE_MIDDLE_BUTTON, vsg::MODKEY_Control, options);
    _settings->bindMouse(ACTION_ROTATE, MOUSE_LEFT_BUTTON | MOUSE_RIGHT_BUTTON, vsg::MODKEY_Control, options);

    options.add(OPTION_SCALE_X, 5.0);
    options.add(OPTION_SCALE_Y, 5.0);

    // zoom with the scroll wheel:
    _settings->bindScroll(ACTION_ZOOM_IN, DIR_UP);
    _settings->bindScroll(ACTION_ZOOM_OUT, DIR_DOWN);

    // pan around with arrow keys:
    _settings->bindKey(ACTION_PAN_LEFT, vsg::KEY_Left);
    _settings->bindKey(ACTION_PAN_RIGHT, vsg::KEY_Right);
    _settings->bindKey(ACTION_PAN_UP, vsg::KEY_Up);
    _settings->bindKey(ACTION_PAN_DOWN, vsg::KEY_Down);

    // double click the left button to zoom in on a point:
    options.clear();
    options.add(OPTION_GOTO_RANGE_FACTOR, 0.4);
    _settings->bindMouseDoubleClick(ACTION_GOTO, MOUSE_LEFT_BUTTON, 0L, options);

    // double click the right button (or CTRL-left button) to zoom out to a point
    options.clear();
    options.add(OPTION_GOTO_RANGE_FACTOR, 2.5);
    _settings->bindMouseDoubleClick(ACTION_GOTO, MOUSE_RIGHT_BUTTON, 0L, options);
    _settings->bindMouseDoubleClick(ACTION_GOTO, MOUSE_LEFT_BUTTON, vsg::MODKEY_Control, options);

    // map multi-touch pinch to a discrete zoom
    options.clear();
    _settings->bindPinch(ACTION_ZOOM, options);

    options.clear();
    _settings->bindTwist(ACTION_ROTATE, options);
    _settings->bindMultiDrag(ACTION_ROTATE, options);

    _settings->lockAzimuthWhilePanning = true;
    _settings->zoomToMouse = true;
}

void
MapManipulator::applySettings(std::shared_ptr<Settings> settings)
{
    if ( settings )
    {
        _settings = settings;
    }
    else
    {
        configureDefaultSettings();
    }

    _task._type = TASK_NONE;

    // apply new pitch restrictions
    double old_pitch_rad;
    getEulerAngles(_state.localRotation, nullptr, &old_pitch_rad);

    double old_pitch_deg = rad2deg(old_pitch_rad);
    double new_pitch_deg = clamp(old_pitch_deg, _settings->minPitch, _settings->maxPitch);

    setDistance(_state.distance);
}

std::shared_ptr<MapManipulator::Settings>
MapManipulator::getSettings() const
{
    return _settings;
}

void
MapManipulator::reinitialize()
{
    _state = State();
    _thrown = false;
    _delta.set(0.0, 0.0);
    _throwDelta.set(0.0, 0.0);
    _continuousDelta.set(0.0, 0.0);
    _continuous = 0;
    _lastAction = ACTION_NULL;
    clearEvents();
}

bool
MapManipulator::createLocalCoordFrame(const vsg::dvec3& worldPos, vsg::dmat4& out_frame) const
{
    if (_worldSRS.valid())
    {
        out_frame = to_vsg(_worldSRS.topocentricToWorldMatrix(to_glm(worldPos)));
    }
    return _worldSRS.valid();
}

void
MapManipulator::setCenter(const vsg::dvec3& worldPos)
{
    _state.center = worldPos;

    if (_worldSRS.isGeocentric())
    {
        glm::dmat4 m = _worldSRS.topocentricToWorldMatrix(to_glm(worldPos));

        // remove the translation component
        _state.centerRotation = to_vsg(m);
        _state.centerRotation[3][0] = 0;
        _state.centerRotation[3][1] = 0;
        _state.centerRotation[3][2] = 0;
    }
}


vsg::dmat4
MapManipulator::getWorldLookAtMatrix(const vsg::dvec3& point) const
{
    //The look vector will be going directly from the eye point to the point on the earth,
    //so the look vector is simply the up vector at the center point
    vsg::dmat4 cf;
    createLocalCoordFrame(point, cf);

    vsg::dvec3 lookVector = -getZAxis(cf);

    vsg::dvec3 side;

    //Force the side vector to be orthogonal to north
    vsg::dvec3 worldUp(0,0,1);

    double ca = fabs(vsg::dot(worldUp, lookVector));
    if (equiv(ca, 1.0))
    {
        //We are looking nearly straight down the up vector, so use the Y vector for world up instead
        worldUp = vsg::dvec3(0, 1, 0);
    }

    side = vsg::cross(lookVector, worldUp);
    vsg::dvec3 up = vsg::cross(side, lookVector);
    up = vsg::normalize(up);

    //We want a very slight offset
    double offset = 1e-6;

    return vsg::lookAt(point - (lookVector * offset), point, up);
}

#if 1
Viewpoint
MapManipulator::viewpoint() const
{
    Viewpoint vp;

#if 0
    // Tethering? Use the tether viewpoint.
    if ( isTethering() && _setVP1.has_value() )
    {
        vp = _setVP1.get();

        if (vp.node)
            vp.focalPoint->fromWorld(_srs.get(), computeWorld(vp.node));
        else
            vp.focalPoint.unset();
    }
#endif

    // the focal point:
    vp.point = GeoPoint(_worldSRS, _state.center);

    // Always update the local offsets.
    double localAzim, localPitch;
    getEulerAngles(_state.localRotation, &localAzim, &localPitch );

    vp.heading = Angle(localAzim, Units::RADIANS).to(Units::DEGREES);
    vp.pitch = Angle(localPitch, Units::RADIANS).to(Units::DEGREES);
    vp.range->set(_state.distance, Units::METERS);

    if ( _state.localPositionOffset.x != 0.0 || _state.localPositionOffset.y != 0.0 || _state.localPositionOffset.z != 0.0 )
    {
        vp.positionOffset = to_glm(_state.localPositionOffset);
    }

    return vp;
}
#endif

void
MapManipulator::setViewpoint(const Viewpoint& vp)
{
    setViewpoint(vp, std::chrono::duration<float>(0.0f));
}

void
MapManipulator::setViewpoint(const Viewpoint& vp, std::chrono::duration<float> duration_seconds)
{    
#if 0
    // Save any existing tether node so we can properly invoke the callback.
    osg::ref_ptr<vsg::Node> oldEndNode;
    if (isTethering() && _tetherCallback.valid())
    {
        oldEndNode = _setVP1->getNode();
    }
#endif

    // starting viewpoint; all fields will be set:
    _state.setVP0 = viewpoint();

    // ending viewpoint
    _state.setVP1 = vp;

    // Reset the tethering offset quat.
    _state.tetherRotationVP0 = _state.tetherRotation;
    _state.tetherRotationVP1 = vsg::dquat();

    // Fill in any missing end-point data with defaults matching the current camera setup.
    // Then all fields are guaranteed to contain usable data during transition.
    double defPitch, defAzim;
    getEulerAngles(_state.localRotation, &defAzim, &defPitch);

    if (!_state.setVP1->heading.has_value())
        _state.setVP1->heading = Angle(defAzim, Units::RADIANS);

    if (!_state.setVP1->pitch.has_value())
        _state.setVP1->pitch = Angle(defPitch, Units::RADIANS);

    if (!_state.setVP1->range.has_value())
        _state.setVP1->range = Distance(_state.distance, Units::METERS);

    if (duration_seconds.count() <= 0.0f)
    {
        duration_seconds = std::chrono::duration<float>(0.0f);
    }

    _state.setVPDuration = duration_seconds;

    // Timed transition, we need to calculate some things:
    if (duration_seconds.count() > 0.0f)
    {
        // Start point is the current manipulator center:
        vsg::dvec3 startWorld;
        vsg::ref_ptr<vsg::Node> startNode = {}; // _setVP0->getNode();
        startWorld = (_state.center); // startNode ? computeWorld(startNode) : _center;

        _state.setVPStartTime.clear();

        // End point is the world coordinates of the target viewpoint:
        auto world_pos = _state.setVP1->position().transform(_worldSRS);
        vsg::dvec3 endWorld(world_pos.x, world_pos.y, world_pos.z);

        // calculate an acceleration factor based on the Z differential.
        _state.setVPArcHeight = 0.0;
        double range0 = _state.setVP0->range->as(Units::METERS);
        double range1 = _state.setVP1->range->as(Units::METERS);

        double pitch0 = _state.setVP0->pitch->as(Units::RADIANS);
        double pitch1 = _state.setVP1->pitch->as(Units::RADIANS);

        double h0 = range0 * sin(-pitch0);
        double h1 = range1 * sin(-pitch1);
        double dh = (h1 - h0);

        // calculate the total distance the focal point will travel and derive an arc height:
        double de = length(endWorld - startWorld);

        // maximum height during viewpoint transition
        if (_settings->arcViewpoints)
        {
            _state.setVPArcHeight = std::max(de - fabs(dh), 0.0);
        }

        // calculate acceleration coefficients
        if (_state.setVPArcHeight > 0.0)
        {
            // if we're arcing, we need separate coefficients for the up and down stages
            double h_apex = 2.0 * (h0 + h1) + _state.setVPArcHeight;
            double dh2_up = fabs(h_apex - h0) / 100000.0;
            _state.setVPAccel = log10(dh2_up);
            double dh2_down = fabs(h_apex - h1) / 100000.0;
            _state.setVPAccel2 = -log10(dh2_down);
        }
        else
        {
            // on arc => simple unidirectional acceleration:
            double dh2 = (h1 - h0) / 100000.0;
            _state.setVPAccel = fabs(dh2) <= 1.0 ? 0.0 : dh2 > 0.0 ? log10(dh2) : -log10(-dh2);
            if (fabs(_state.setVPAccel) < 1.0) _state.setVPAccel = 0.0;
        }

#if 0
        // Adjust the duration if necessary.
        if (_settings->getAutoViewpointDurationEnabled())
        {
            double maxDistance = _worldSRS.ellipsoid().semiMajorAxis();
            double ratio = clamp(de / maxDistance, 0.0, 1.0);
            ratio = accelerationInterp(ratio, -4.5);
            double minDur, maxDur;
            _settings->getAutoViewpointDurationLimits(minDur, maxDur);
            _state.setVPDuration.set(minDur + ratio * (maxDur - minDur), Units::SECONDS);
        }
#endif
    }

    else
    {
        // Immediate transition? Just do it now.
        _state.setVPStartTime = _previousTime;
        //_state.setVPStartTime->set( _time_s_now, Units::SECONDS );
        setViewpointFrame(_previousTime);
    }

#if 0
    // Fire a tether callback if required.
    if (_tetherCallback.valid())
    {
        // starting a tether to a NEW node:
        if (isTethering() && oldEndNode.get() != endNode.get())
            (*_tetherCallback)(endNode.get());

        // breaking a tether:
        else if (!isTethering() && oldEndNode.valid())
            (*_tetherCallback)(0L);
    }
#endif


    // reset other global state flags.
    _thrown      = false;
    _task._type = TASK_NONE;
}

// returns "t" [0..1], the interpolation coefficient.
double
MapManipulator::setViewpointFrame(const vsg::time_point& now)
{
    if ( !_state.setVPStartTime.has_value() )
    {
        _state.setVPStartTime = now;
        return 0.0;
    }
    else
    {
        // Start point is the current manipulator center:
        auto p0 = _state.setVP0->position().transform(_worldSRS);
        vsg::dvec3 startWorld(p0.x, p0.y, p0.z);

        // End point is the world coordinates of the target viewpoint:
        auto p1 = _state.setVP1->position().transform(_worldSRS);
        vsg::dvec3 endWorld(p1.x, p1.y, p1.z);

        // Remaining time is the full duration minus the time since initiation:
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(
            now - _state.setVPStartTime.value());

        auto duration = _state.setVPDuration;

        double t = (double)elapsed.count() / (double)duration.count();
        t = std::min(t, 1.0);

        double tp = t;

        if (_state.setVPArcHeight > 0.0 )
        {
            if ( tp <= 0.5 )
            {
                double t2 = 2.0*tp;
                tp = 0.5*t2;
            }
            else
            {
                double t2 = 2.0*(tp-0.5);
                tp = 0.5+(0.5*t2);
            }

            // the more smoothsteps you do, the more pronounced the fade-in/out effect
            smoothStepInterp( tp );
        }
        else if ( t > 0.0 )
        {
            tp = smoothStepInterp( tp );
        }

        vsg::dvec3 newCenter =
            _worldSRS.isGeocentric() ? nlerp(startWorld, endWorld, tp) :
            lerp(startWorld, endWorld, tp);

        // Calculate the delta-heading, and make sure we are going in the shortest direction:
        Angle d_azim = _state.setVP1->heading.value() - _state.setVP0->heading.value();
        if ( d_azim.as(Units::RADIANS) > M_PI )
            d_azim = d_azim - Angle(2.0*M_PI, Units::RADIANS);
        else if ( d_azim.as(Units::RADIANS) < -M_PI )
            d_azim = d_azim + Angle(2.0*M_PI, Units::RADIANS);
        double newAzim = _state.setVP0->heading->as(Units::RADIANS) + tp*d_azim.as(Units::RADIANS);

        // Calculate the new pitch:
        Angle d_pitch = _state.setVP1->pitch.value() - _state.setVP0->pitch.value();
        double newPitch = _state.setVP0->pitch->as(Units::RADIANS) + tp*d_pitch.as(Units::RADIANS);

        // Calculate the new range:
        Distance d_range = _state.setVP1->range.value() - _state.setVP0->range.value();
        double newRange =
            _state.setVP0->range->as(Units::METERS) +
            d_range.as(Units::METERS)*tp + sin(M_PI*tp)* _state.setVPArcHeight;

        // Calculate the offsets
        vsg::dvec3 offset0 = to_vsg(_state.setVP0->positionOffset.value_or(glm::dvec3{ 0,0,0 }));
        vsg::dvec3 offset1 = to_vsg(_state.setVP1->positionOffset.value_or(glm::dvec3{ 0,0,0 }));
        vsg::dvec3 newOffset = offset0 + (offset1-offset0)*tp;

        // Activate.
        //setLookAt(newCenter, newAzim, newPitch, newRange, newOffset);
        setCenter(newCenter);
        setDistance(newRange);
        //_state.center = newCenter;
        //_state.distance = newRange;
        _state.localRotation = getQuaternion(newAzim, newPitch);
        _state.localPositionOffset = newOffset;

        // interpolate tether rotation:
        _state.tetherRotation = vsg::mix(_state.tetherRotationVP0, _state.tetherRotationVP1, tp);

        // At t=1 the transition is complete.
        if ( t >= 1.0 )
        {
            _state.setVP0.reset();

            // If this was a transition into a tether, keep the endpoint around so we can
            // continue tracking it.
            if ( !isTethering() )
            {
                _state.setVP1.reset();
            }
        }

        return tp;
    }
}

void
MapManipulator::clearViewpoint()
{
    // Cancel any ongoing transition or tethering:
    _state.setVP0.reset();
    _state.setVP1.reset();

    if (!recalculateCenterAndDistanceFromLookVector())
    {
        home();
    }

#if 0
    // Fire the callback to indicate a tethering break.
    if ( _tetherCallback.valid() && breakingTether )
        (*_tetherCallback)( 0L );
#endif
}

vsg::ref_ptr<MapNode>
MapManipulator::getMapNode() const
{
    return _mapNode_weakptr.ref_ptr();
}

bool
MapManipulator::intersect(const vsg::dvec3& start, const vsg::dvec3& end, vsg::dvec3& out_intersection) const
{
    auto mapNode = _mapNode_weakptr.ref_ptr();
    if (mapNode)
    {
        vsg::LineSegmentIntersector lsi(start, end);

        mapNode->terrainNode->accept(lsi);

        if (!lsi.intersections.empty())
        {
            auto closest = std::min_element(
                lsi.intersections.begin(), lsi.intersections.end(),
                [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });

            out_intersection = closest->get()->worldIntersection;
            return true;
        }
    }
    return false;
}

bool
MapManipulator::intersectAlongLookVector(vsg::dvec3& out_world) const
{
    auto mapNode = getMapNode();
    if (mapNode)
    {
        vsg::LookAt lookat;
        lookat.set(_viewMatrix);

        return intersect(
            lookat.eye,
            (lookat.center - lookat.eye) * _state.distance * 1.5,
            out_world);
    }

    return false;
}

void
MapManipulator::home()
{
    // emulate clearViewpoint() without calling it (possible recursion)
    _state.setVP0.reset();
    _state.setVP1.reset();

    _state.localRotation.set(0, 0, 0, 1);

    double radius;
    if (_worldSRS.isGeocentric())
    {
        radius = _worldSRS.ellipsoid().semiMajorAxis();
        setCenter(vsg::dvec3(radius, 0, 0));
    }
    else
    {
        radius = _worldSRS.bounds().width() * 0.5;
        setCenter(vsg::dvec3(0, 0, 0));
    }

    setDistance(radius * 3.5);

    clearEvents();
}

void
MapManipulator::clearEvents()
{
    _continuous = 0;
    _keyPress.clear();
    _buttonPress.clear();
    // Note: never clear the _previousMove event!
    _task.reset();
    _dirty = true;
}

void
MapManipulator::apply(vsg::KeyPressEvent& keyPress)
{
    if (keyPress.handled || !withinRenderArea(_previousMove))
        return;

    _keyPress = keyPress;

    recalculateCenterAndDistanceFromLookVector();

    //std::cout << "KeyPressEvent" << std::endl;

    _lastAction = _settings->getAction(
        EVENT_KEY_DOWN,
        keyPress.keyBase,
        keyPress.keyModifier);

    if (handleKeyboardAction(_lastAction, keyPress.time, 0))
    {
        keyPress.handled = true;
    }
}

void
MapManipulator::apply(vsg::KeyReleaseEvent& keyRelease)
{
    //std::cout << "KeyReleaseEvent" << std::endl;

    _keyPress.clear();
}

void
MapManipulator::apply(vsg::ButtonPressEvent& buttonPress)
{
    if (buttonPress.handled || !withinRenderArea(buttonPress))
        return;

    //std::cout << "ButtonPressEvent" << std::endl;

    // simply record the button press event.
    clearEvents();

    _buttonPress = buttonPress;

    recalculateCenterAndDistanceFromLookVector();

    buttonPress.handled = true;
}

void
MapManipulator::apply(vsg::ButtonReleaseEvent& buttonRelease)
{
    //std::cout << "ButtonReleaseEvent" << std::endl;

    if (isMouseClick(buttonRelease))
    {
        _lastAction = _settings->getAction(
            EVENT_MOUSE_CLICK,
            _buttonPress->button,
            _buttonPress->mask);

        if (handlePointAction(_lastAction, buttonRelease.x, buttonRelease.y, buttonRelease.time))
            _dirty = true;

        vsg::dvec3 world;
        viewportToWorld(buttonRelease.x, buttonRelease.y, world);
    }

    clearEvents();

    buttonRelease.handled = true;
}

void
MapManipulator::apply(vsg::MoveEvent& moveEvent)
{
    // Note: always record the move event (into _previousMove) regardless
    // of whether we process the new move event.

    // If there's no button press, bail out.
    if (!_buttonPress.has_value())
    {
        _previousMove = moveEvent;
        moveEvent.handled = true;
        return;
    }

    // Check if the button got released outside the window (and did not generate an event)
    if (moveEvent.mask == 0 && _previousMove->mask != 0)
    {
        _previousMove = moveEvent;
        clearEvents();
        return;
    }

    // Good to go, process the move:
    vsg::ref_ptr<vsg::Window> window = moveEvent.window;

    _lastAction = _settings->getAction(
        EVENT_MOUSE_DRAG,
        moveEvent.mask, // button mask
        _keyPress.has_value() ? _keyPress->keyModifier : 0);

    bool wasContinuous = _continuous > 0;
    if (_lastAction.getBoolOption(OPTION_CONTINUOUS, false))
        ++_continuous;
    else
        _continuous = 0;

    if (handleMouseAction(_lastAction, _previousMove.value(), moveEvent))
        _dirty = true;

    if (_continuous > 0) // && !wasContinuous)
    {
        _continuousAction = _lastAction;
        _context->requestFrame();
        _dirty = true;
    }

    _thrown = false;
    moveEvent.handled = true;

    _previousMove = moveEvent;
}

void
MapManipulator::apply(vsg::ScrollWheelEvent& scrollEvent)
{
    if (scrollEvent.handled || !withinRenderArea(_previousMove))
        return;

    //std::cout << "ScrollWheelEvent" << std::endl;

    Direction dir =
        scrollEvent.delta.x < 0 ? DIR_LEFT :
        scrollEvent.delta.x > 0 ? DIR_RIGHT :
        scrollEvent.delta.y < 0 ? DIR_UP :
        scrollEvent.delta.y > 0 ? DIR_DOWN :
        DIR_NA;

    _lastAction = _settings->getAction(
        EVENT_SCROLL,
        dir,
        _keyPress.has_value() ? _keyPress->keyModifier : 0);

    handleScrollAction(
        _lastAction,
        scrollEvent.time,
        _lastAction.getDoubleOption(OPTION_DURATION, 0.2));
}

void
MapManipulator::apply(vsg::TouchDownEvent& touchDown)
{
    NOT_YET_IMPLEMENTED("");
}

void
MapManipulator::apply(vsg::TouchUpEvent& touchUp)
{
    NOT_YET_IMPLEMENTED("");
}

void
MapManipulator::apply(vsg::TouchMoveEvent& touchMove)
{
    NOT_YET_IMPLEMENTED("");
}

void
MapManipulator::apply(vsg::FrameEvent& frame)
{
    //Log()->warn(util::make_string() << "FrameEvent-------------------------- " << frame.time.time_since_epoch().count());

    if (_continuous > 0)
    {
        // start requesting frame draws:
        _dirty = true;

        if (_continuous > 1)
        {
            // wait one frame before updating the continuous movement,
            // becuase the first one (in on-demand mode) will have a bogus time delta
            // which will cause a jump.
            double t_factor = to_seconds(frame.time - _previousTime) * 60.0;
            handleMovementAction(_continuousAction._type, _continuousDelta * t_factor);
        }
    }
    else
    {
        _continuousDelta.set(0.0, 0.0);
    }

    serviceTask(frame.time);

    if (isSettingViewpoint())
    {
        setViewpointFrame(frame.time);
    }

    if (isTethering())
    {
        updateTether(frame.time);
    }

    bool camera_changed = updateCamera();

    _previousTime = frame.time;

    // if anything caused the camera's matrix to change, dirty the instance to
    // request a new frame.
    if (camera_changed || _dirty)
    {
        if (_context)
        {
            _context->requestFrame();
        }
        _dirty = false;
    }
}

bool
MapManipulator::updateCamera()
{
    bool changed = false;
    auto camera = _camera_weakptr.ref_ptr();
    if (camera)
    {
        auto oldMatrix = _viewMatrix;

        _viewMatrix =
            vsg::translate(_state.center) *
            _state.centerRotation *
            vsg::translate(_state.localPositionOffset) *
            vsg::rotate(_state.tetherRotation) *
            vsg::rotate(_state.localRotation) *
            vsg::translate(0.0, 0.0, _state.distance);

        auto lookat = camera->viewMatrix.cast<vsg::LookAt>();
        if (!lookat)
        {
            lookat = vsg::LookAt::create();
            camera->viewMatrix = lookat;
        }

        lookat->set(_viewMatrix);

        changed = oldMatrix != _viewMatrix;
    }
    return changed;
}

bool
MapManipulator::serviceTask(vsg::time_point now)
{
    if (_task._type != TASK_NONE)
    {
        _dirty = true;

        // delay the servicing by one frame to prevent jumps in on-demand mode
        if (_task._frameCount++ > 0)
        {
            auto dt = to_seconds(now - _previousTime);
            if (dt > 0.0)
            {
                // cap the DT so we don't exceed the expected delta.
                //dt = std::min(dt, _task._duration_s);

                double dx = _task._delta.x * dt;
                double dy = _task._delta.y * dt;

                switch (_task._type)
                {
                case TASK_PAN:
                    pan(dx, dy);
                    break;
                case TASK_ROTATE:
                    rotate(dx, dy);
                    break;
                case TASK_ZOOM:
                    zoom(dx, dy);
                    break;
                default:
                    break;
                }

                _task._duration_s -= dt;

                if (_task._duration_s <= 0.0)
                {
                    _task._type = TASK_NONE;
                }
            }
        }
    }

    // returns true if the task is still running.
    return _task._type != TASK_NONE;
}


bool
MapManipulator::isMouseClick(vsg::ButtonReleaseEvent& buttonRelease) const
{
    if (!_buttonPress.has_value()) // || !_buttonRelease.has_value())
        return false;

    static const float velocity = 0.1f;

    auto down = ndc(_buttonPress.value());
    auto up = ndc(buttonRelease);

    float dx = up.x - down.x;
    float dy = up.y - down.y;
    float len = sqrtf(dx * dx + dy * dy);
    auto dtmillis = std::chrono::duration_cast<std::chrono::milliseconds>(buttonRelease.time - _buttonPress->time);
    float dt = (float)dtmillis.count() * 0.001f;
    return (len < dt* velocity);
}

bool
MapManipulator::recalculateCenterFromLookVector()
{
    auto camera = _camera_weakptr.ref_ptr();
    if (!camera)
        return false;

    vsg::LookAt lookat;
    lookat.set(camera->viewMatrix->inverse());
    auto look = vsg::normalize(lookat.center - lookat.eye);
    
    bool ok = false;
    vsg::dvec3 intersection;

    if (intersectAlongLookVector(intersection))
    {
        ok = true;
    }

    else if (intersect(lookat.eye, lookat.eye + look * _state.distance * 1.5, intersection))
    {
        ok = true;
    }

    // backup plan, intersect the ellipsoid or the ground plane
    else if (_worldSRS.isGeocentric())
    {
        glm::dvec3 i;
        auto target = lookat.eye + look * 1e10;
        if (_worldSRS.ellipsoid().intersectGeocentricLine(to_glm(lookat.eye), to_glm(target), i))
        {
            intersection = to_vsg(i);
            ok = true;
        }
    }

    else
    {
        // simple line/plane intersection
        vsg::dvec3 P0(0, 0, 0); // point on the plane
        vsg::dvec3 N(0, 0, 1); // normal to the plane
        vsg::dvec3 L = look; // unit direction of the line
        vsg::dvec3 L0 = lookat.eye; // point on the line
        auto LdotN = vsg::dot(L, N);
        if (equiv(LdotN, 0)) return false; // parallel
        auto D = vsg::dot((P0 - L0), N) / LdotN;
        if (D < 0) return false; // behind the camera
        intersection = L0 + L * D;
        ok = true;
    }

    if (ok)
    {
        if (_worldSRS.isGeocentric())
        {
            // keep the existing center, but change its length
            double len = vsg::length(intersection);
            _state.center = vsg::normalize(_state.center) * len;
        }
        else
        {
            setCenter(intersection);
        }
    }

    return ok;
}

bool
MapManipulator::recalculateCenterAndDistanceFromLookVector()
{
    auto camera = _camera_weakptr.ref_ptr();
    if (!camera)
        return false;

    vsg::LookAt lookat;
    lookat.set(camera->viewMatrix->inverse());
    auto look = vsg::normalize(lookat.center - lookat.eye);

    bool ok = false;
    vsg::dvec3 intersection;
    double dist = length(lookat.eye);

    if (intersect(lookat.eye, lookat.eye + look * dist, intersection))
    {
        ok = true;
    }

    // backup plan, intersect the ellipsoid or the ground plane
    else if (_worldSRS.isGeocentric())
    {
        glm::dvec3 i;
        auto target = lookat.eye + look * 1e10;
        if (_worldSRS.ellipsoid().intersectGeocentricLine(to_glm(lookat.eye), to_glm(target), i))
        {
            intersection = to_vsg(i);
            ok = true;
        }
    }

    else
    {
        // simple line/plane intersection
        vsg::dvec3 P0(0, 0, 0); // point on the plane
        vsg::dvec3 N(0, 0, 1); // normal to the plane
        vsg::dvec3 L = look; // unit direction of the line
        vsg::dvec3 L0 = lookat.eye; // point on the line
        auto LdotN = vsg::dot(L, N);
        if (equiv(LdotN, 0)) return false; // parallel
        auto D = vsg::dot((P0 - L0), N) / LdotN;
        if (D < 0) return false; // behind the camera
        intersection = L0 + L * D;
        ok = true;
    }

    if (ok)
    {
        setCenter(intersection);
        setDistance(vsg::length(intersection - lookat.eye));
    }

    return ok;
}

void
MapManipulator::pan(double dx, double dy)
{
    // to pan, we need a focus point on the terrain:
    //if ( !recalculateCenterFromLookVector() )
    //    return;

    auto camera = _camera_weakptr.ref_ptr();
    if (!camera)
        return;

    double scale = -0.3 * _state.distance;

    // the view-space coordinate frame:
    auto lookat = camera->viewMatrix->inverse();
    auto x_axis = vsg::normalize(getXAxis(lookat));
    auto y_axis = vsg::normalize(cross(getZAxis(_state.centerRotation), x_axis));

    auto dv = (x_axis * dx * scale) + (y_axis * dy * scale);

    // save the previous CF so we can do azimuth locking:
    //vsg::dmat4 oldCenterLocalToWorld = _centerReferenceFrame; // _centerLocalToWorld;

    // move the center point
    double old_len = vsg::length(_state.center);
    vsg::dvec3 new_center = _state.center + dv;

    if (_worldSRS.isGeocentric())
    {
        // in geocentric, ensure that it doesn't change length.
        new_center = vsg::normalize(new_center);
        new_center *= old_len;
    }

    setCenter(new_center);

#if 0
        if ( _settings->getLockAzimuthWhilePanning() )
        {
            // in azimuth-lock mode, _centerRotation maintains a consistent north vector
            _centerRotation = computeCenterRotation( _center );
        }

        else
        {
            // otherwise, we need to rotate _centerRotation manually.
            vsg::dvec3 new_localUp = getUpVector( _centerLocalToWorld );

            osg::Quat pan_rotation;
            pan_rotation.makeRotate( localUp, new_localUp );

            if ( !pan_rotation.zeroRotation() )
            {
                _centerRotation = _centerRotation * pan_rotation;
                _previousUp = new_localUp;
            }
        }
#endif
#if 0
    }
    else
    {
    double scale = _distance;

    // Panning in tether mode changes the focal view offsets.
    _viewOffset.x() -= dx * scale;
    _viewOffset.y() -= dy * scale;

    //Clamp values within range
    _viewOffset.x() = clamp(_viewOffset.x(), -_settings->getMaxXOffset(), _settings->getMaxXOffset());
    _viewOffset.y() = clamp(_viewOffset.y(), -_settings->getMaxYOffset(), _settings->getMaxYOffset());
    }
#endif

    //collisionDetect();
}

void
MapManipulator::rotate(double dx, double dy)
{
    // clamp the local pitch delta; never allow the pitch to hit -90.
    double minp = deg2rad(std::min(_settings->minPitch, -89.9));
    double maxp = deg2rad(std::max(_settings->maxPitch, -0.1));

    // clamp pitch range:
    double oldPitch;
    getEulerAngles(_state.localRotation, 0L, &oldPitch);

    if ( dy + oldPitch > maxp || dy + oldPitch < minp )
        dy = 0;

    vsg::dmat4 rotationFrame = vsg::rotate(_state.localRotation);
    vsg::dvec3 tangent = getXAxis(rotationFrame);
    vsg::dvec3 up = vsg::dvec3(0, 0, 1);

    vsg::dquat rotate_elevation(dy, tangent);
    vsg::dquat rotate_azim(-dx, up);

    _state.localRotation = _state.localRotation * rotate_elevation * rotate_azim;

    //collisionDetect();

    //recalculateCenterAndDistanceFromLookVector();
}

namespace
{
    // adapted from openscenegraph
    template<typename Q>
    vsg::dquat slerp(double t, const Q& from, const Q& to)
    {
        const double epsilon = 0.00001;
        double omega, cosomega, sinomega, scale_from, scale_to;

        Q quatTo(to);
        
        // this is a dot product
        glm::dvec4 a(from[0], from[1], from[2], from[3]);
        glm::dvec4 b(to[0], to[1], to[2], to[3]);
        cosomega = glm::dot(a, b);

        if (cosomega < 0.0)
        {
            cosomega = -cosomega;
            quatTo = -to;
        }

        if ((1.0 - cosomega) > epsilon)
        {
            omega = acos(cosomega);  // 0 <= omega <= Pi (see man acos)
            sinomega = sin(omega);  // this sinomega should always be +ve so
            // could try sinomega=sqrt(1-cosomega*cosomega) to avoid a sin()?
            scale_from = sin((1.0 - t) * omega) / sinomega;
            scale_to = sin(t * omega) / sinomega;
        }
        else
        {
            scale_from = 1.0 - t;
            scale_to = t;
        }

        return (from * scale_from) + (quatTo * scale_to);
    }
}

void
MapManipulator::zoom(double dx, double dy)
{
    //if (isTethering())
    //{
    //    double scale = 1.0f + dy;
    //    setDistance(_distance * scale);
    //    collisionDetect();
    //    return;
    //}

    if (_settings->zoomToMouse == true && dy < 0.0)
    {
        vsg::dvec3 target;

        std::int32_t x = _buttonPress.has_value() ? _buttonPress->x : _previousMove->x;
        std::int32_t y = _buttonPress.has_value() ? _buttonPress->y : _previousMove->y;

        if (viewportToWorld(x, y, target))
        {
            //recalculateCenterFromLookVector();

            // Calcuate a rotation that we'll use to interpolate from our center point to the target
            vsg::dquat rotCenterToTarget;
            // Check if the vectors are too close to avoid NaN in quaternion calculation
            double dist = distance3D(_state.center, target);
            double centerMag = vsg::length(_state.center);
            double relativeDist = centerMag > 0 ? dist / centerMag : 0;
            if (relativeDist < 1e-6) {
                // Use identity quaternion when vectors are nearly identical (relative to their magnitude)
                rotCenterToTarget = vsg::dquat(0, 0, 0, 1);
            } else {
                rotCenterToTarget.set(_state.center, target);
            }

            // Factor by which to scale the distance:
            double scale = 1.0f + dy;
            double newDistance = _state.distance * scale;
            double delta = _state.distance - newDistance;
            double ratio = delta / _state.distance;

            if (_worldSRS.isGeocentric())
            {
                // xform target point into the current focal point's local frame,
                // and adjust the zoom ratio to account for the difference in 
                // target distance based on the earth's curvature...approximately!
                vsg::dvec3 targetInLocalFrame = vsg::inverse(_state.centerRotation) * target;
                double crRatio = vsg::length(_state.center) / targetInLocalFrame.z;
                ratio *= crRatio;

                // Interpolate a new focal point:
                vsg::dquat rot = slerp(ratio, vsg::dquat(0, 0, 0, 1), rotCenterToTarget);
                setCenter(rot * _state.center);
            }
            else
            {
                // linear interp.
                setCenter(_state.center + (target - _state.center) * ratio);
            }

            // and set the new zoomed distance.
            setDistance(newDistance);

            return;
        }
    }

    //recalculateCenterFromLookVector();
    double scale = 1.0f + dy;
    setDistance(_state.distance * scale);
}

bool
MapManipulator::viewportToWorld(float x, float y, vsg::dvec3& out_world) const
{
    vsg::ref_ptr<vsg::Camera> camera = _camera_weakptr;
    if (!camera)
        return false;

    vsg::LineSegmentIntersector lsi(*camera, x, y);
    getMapNode()->terrainNode->accept(lsi);
    if (lsi.intersections.empty())
        return false;

    auto closest = std::min_element(
        lsi.intersections.begin(), lsi.intersections.end(),
        [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });

    out_world = closest->get()->worldIntersection;

    return true;
}

void
MapManipulator::setDistance(double distance)
{
    _state.distance = clamp(distance, _settings->minDistance, _settings->maxDistance);
}

void
MapManipulator::handleMovementAction(const ActionType& type, vsg::dvec2 d)
{
    switch (type)
    {
    case ACTION_PAN:
        pan(d.x, d.y);
        break;

    case ACTION_ROTATE:
        // in "single axis" mode, zero out one of the deltas.
        if (_continuous && _settings->singleAxisRotation)
        {
            if (::fabs(d.x) > ::fabs(d.y))
                d.y = 0.0;
            else
                d.x = 0.0;
        }
        rotate(d.x, d.y);
        break;

    case ACTION_ZOOM:
        zoom(d.x, d.y);
        break;

    default:
        break;
    }
}

bool
MapManipulator::handlePointAction(
    const Action& action, 
    float mx, float my,
    vsg::time_point time)
{
    if (action._type == ACTION_NULL)
        return true;

    vsg::dvec3 point;
    if (viewportToWorld(mx, my, point))
    {
        switch (action._type)
        {
        case ACTION_GOTO:
#if 0
            Viewpoint here = getViewpoint();
            here.focalPoint()->fromWorld(_srs.get(), point);

            double duration_s = action.getDoubleOption(OPTION_DURATION, 1.0);
            double range_factor = action.getDoubleOption(OPTION_GOTO_RANGE_FACTOR, 1.0);

            here.range() = here.range().get() * range_factor;

            setViewpoint(here, duration_s);
#endif
            break;

        default:
            break;
        }
    }
    return true;
}

void
MapManipulator::applyOptionsToDeltas(const Action& action, vsg::dvec2& d)
{
    d.x *= action.getDoubleOption(OPTION_SCALE_X, 1.0);
    d.y *= action.getDoubleOption(OPTION_SCALE_Y, 1.0);

    if (action.getBoolOption(OPTION_SINGLE_AXIS, false) == true)
    {
        if (fabs(d.x) > fabs(d.y))
            d.y = 0.0;
        else
            d.x = 0.0;
    }
}

bool
MapManipulator::handleMouseAction(
    const Action& action,
    const vsg::MoveEvent& previousMove,
    const vsg::MoveEvent& currentMove)
{
    auto curr = ndc(currentMove);

    if (_continuous && _buttonPress.has_value())
    {
        auto start = ndc(_buttonPress);
        vsg::dvec2 delta(curr.x - start.x, -(curr.y - start.y));
        delta *= 0.1;
        delta *= _settings->mouseSensitivity;
        applyOptionsToDeltas(action, delta);
        _continuousDelta = delta;
    }
    else
    {
        auto prev = ndc(previousMove);
        vsg::dvec2 delta(curr.x - prev.x, -(curr.y - prev.y));
        delta *= _settings->mouseSensitivity;
        applyOptionsToDeltas(action, delta);
        _delta = delta;
        handleMovementAction(action._type, delta);
    }

    return true;
}

bool
MapManipulator::handleMouseClickAction(
    const Action& action,
    vsg::time_point time)
{
    return false;
}

bool
MapManipulator::handleKeyboardAction(
    const Action& action,
    vsg::time_point now,
    double duration)
{
    vsg::dvec2 d(0.0, 0.0);

    switch (action._dir)
    {
    case DIR_LEFT:  d.x = 1; break;
    case DIR_RIGHT: d.x = -1; break;
    case DIR_UP:    d.y = -1; break;
    case DIR_DOWN:  d.y = 1; break;
    default: break;
    }

    d.x *= _settings->keyboardSesitivity;
    d.y *= _settings->keyboardSesitivity;

    applyOptionsToDeltas(action, d);

    return handleAction(action, d, now, duration);
}

bool
MapManipulator::handleScrollAction(
    const Action& action,
    vsg::time_point time,
    double duration)
{
    const double scrollFactor = 1.5;

    vsg::dvec2 d(0.0, 0.0);

    switch (action._dir)
    {
    case DIR_LEFT:  d.x = 1; break;
    case DIR_RIGHT: d.x = -1; break;
    case DIR_UP:    d.y = -1; break;
    case DIR_DOWN:  d.y = 1; break;
    default: break;
    }

    d.x *= scrollFactor * _settings->scrollSensitivity;
    d.y *= scrollFactor * _settings->scrollSensitivity;

    applyOptionsToDeltas(action, d);

    return handleAction(action, d, time, duration);
}

bool
MapManipulator::handleAction(
    const Action& action,
    const vsg::dvec2& d,
    vsg::time_point time,
    double duration)
{
    bool handled = true;

    switch( action._type )
    {
#if 0
    case ACTION_HOME:
        if ( _homeViewpoint.has_value() )
        {
            setViewpoint( _homeViewpoint.value(), _homeViewpointDuration );
        }
        break;
#endif
    case ACTION_HOME:
        home();
        break;

    case ACTION_PAN:
    case ACTION_PAN_LEFT:
    case ACTION_PAN_RIGHT:
    case ACTION_PAN_UP:
    case ACTION_PAN_DOWN:
        _task.set(TASK_PAN, d, duration, time);
        break;

    case ACTION_ROTATE:
    case ACTION_ROTATE_LEFT:
    case ACTION_ROTATE_RIGHT:
    case ACTION_ROTATE_UP:
    case ACTION_ROTATE_DOWN:
        _task.set(TASK_ROTATE, d, duration, time);
        break;

    case ACTION_ZOOM:
    case ACTION_ZOOM_IN:
    case ACTION_ZOOM_OUT:
        _task.set(TASK_ZOOM, d, duration, time);
        break;

    default:
        handled = false;
    }

    return handled;
}

void
MapManipulator::recalculateRoll()
{
#if 0
    vsg::dmat4 centerRotation = extract_rotation(_centerReferenceFrame);

    vsg::dvec3 lookVector = -getUpVector(centerRotation);
    vsg::dvec3 upVector = getFrontVector(centerRotation);

    vsg::dvec3 localUp = getUpVector(_centerReferenceFrame);

    vsg::dvec3 sideVector = vsg::cross(lookVector, localUp);

    if (vsg::length(sideVector) < 0.1)
    {
        //ROCKY_INFO<<"Side vector short "<<sideVector.length()<<std::endl;

        sideVector = vsg::normalize(vsg::cross(upVector, localUp));

    }

    vsg::dvec3 newUpVector = vsg::normalize(vsg::cross(sideVector, lookVector));

    vsg::dquat rotate_roll(upVector, newUpVector);

    if (!rotate_roll.zeroRotation())
    {
        _centerRotation = _centerRotation * rotate_roll;
    }
#endif
}

void
MapManipulator::compositeEulerAngles(double* out_azim, double* out_pitch) const
{
    vsg::dvec3 look = vsg::normalize(-getZAxis(_state.centerRotation));
    vsg::dvec3 up = vsg::normalize(getYAxis(_state.centerRotation));

    if (out_azim)
    {
        if (look.z < -0.9)
            *out_azim = atan2(up.x, up.y);
        else if (look.z > 0.9)
            *out_azim = atan2(-up.x, -up.y);
        else
            *out_azim = atan2(look.x, look.y);

        *out_azim = normalizeAzimRad(*out_azim);
    }

    if (out_pitch)
    {
        *out_pitch = asin(look.z);
    }
}


// Extracts azim and pitch from a quaternion that does not contain any roll.
void
MapManipulator::getEulerAngles(const vsg::dquat& q, double* out_azim, double* out_pitch) const
{
    vsg::dmat4 m = vsg::rotate( q );

    vsg::dvec3 look = vsg::normalize(-getZAxis(m));
    vsg::dvec3 up = vsg::normalize(getYAxis(m));

    if ( out_azim )
    {
        if (look.z < -0.9)
            *out_azim = atan2(up.x, up.y);
        else if (look.z > 0.9)
            *out_azim = atan2(-up.x, -up.y);
        else
            *out_azim = atan2(look.x, look.y);

        *out_azim = normalizeAzimRad(*out_azim);
    }

    if (out_pitch)
    {
        *out_pitch = asin(look.z);
    }
}

vsg::dquat
MapManipulator::getQuaternion(double azim, double pitch) const
{
    vsg::dquat azim_q(azim, vsg::dvec3(0, 0, 1));
    vsg::dquat pitch_q(-pitch - (0.5 * M_PI), vsg::dvec3(1, 0, 0));
    return vsg::inverse(azim_q * pitch_q);
    //vsg::dmat4 newRot = vsg::inverse(azim_q * pitch_q);
    //return vsg::rotate(vsg::inverse(newRot)); .getRotate();
}

/// code adopted from VSG
bool
MapManipulator::withinRenderArea(const vsg::PointerEvent& pointerEvent) const
{
    if (_window_weakptr != pointerEvent.window)
        return false;

    auto camera = _camera_weakptr.ref_ptr();
    if (!camera)
        return false;

    auto renderArea = camera->getRenderArea();

    return
        pointerEvent.x >= renderArea.offset.x && 
        pointerEvent.x < static_cast<int32_t>(renderArea.offset.x + renderArea.extent.width) &&
        pointerEvent.y >= renderArea.offset.y &&
        pointerEvent.y < static_cast<int32_t>(renderArea.offset.y + renderArea.extent.height);
}

/// compute non dimensional window coordinate (-1,1) from event coords
/// code adopted from VSG
vsg::dvec2
MapManipulator::ndc(const vsg::PointerEvent& event) const
{
    auto camera = _camera_weakptr.ref_ptr();
    if (!camera)
        false;

    auto renderArea = camera->getRenderArea();

    double aspectRatio = static_cast<double>(renderArea.extent.width) / static_cast<double>(renderArea.extent.height);
    vsg::dvec2 v(
        (renderArea.extent.width > 0) ? (static_cast<double>(event.x - renderArea.offset.x) / static_cast<double>(renderArea.extent.width) * 2.0 - 1.0) * aspectRatio : 0.0,
        (renderArea.extent.height > 0) ? static_cast<double>(event.y - renderArea.offset.y) / static_cast<double>(renderArea.extent.height) * 2.0 - 1.0 : 0.0);

    return v;
}



void
MapManipulator::updateTether(const vsg::time_point& t)
{
    // Initial transition is complete, so update the camera for tether.
    if (_state.setVP1->pointFunction)
    {
        auto pos = _state.setVP1->position();

        auto p0 = pos.transform(_worldSRS);
        vsg::dvec3 world(p0.x, p0.y, p0.z);

        // If we just called setViewpointFrame, no need to calculate the center again.
        if (!isSettingViewpoint())
        {
            setCenter(world);
        };

        if (_settings->tetherMode == TETHER_CENTER)
        {
            if (_state.lastTetherMode == TETHER_CENTER_AND_ROTATION)
            {
                //TODO
                //// level out the camera so we don't leave the camera is weird state.
                //vsg::dmat4 localToFrame(L2W * vsg::dmat4::inverse(_centerLocalToWorld));
                //double azim = atan2(-localToFrame(0, 1), localToFrame(0, 0));
                //_tetherRotation.makeRotate(-azim, 0.0, 0.0, 1.0);
            }
        }
#if 0
        else
        {
            // remove any scaling introduced by the model
            double sx = 1.0 / sqrt(L2W(0, 0) * L2W(0, 0) + L2W(1, 0) * L2W(1, 0) + L2W(2, 0) * L2W(2, 0));
            double sy = 1.0 / sqrt(L2W(0, 1) * L2W(0, 1) + L2W(1, 1) * L2W(1, 1) + L2W(2, 1) * L2W(2, 1));
            double sz = 1.0 / sqrt(L2W(0, 2) * L2W(0, 2) + L2W(1, 2) * L2W(1, 2) + L2W(2, 2) * L2W(2, 2));
            L2W = L2W * osg::Matrixd::scale(sx, sy, sz);

            if (_settings->getTetherMode() == TETHER_CENTER_AND_HEADING)
            {
                // Back out the tetheree's rotation, then discard all but the heading component:
                osg::Matrixd localToFrame(L2W * osg::Matrixd::inverse(_centerLocalToWorld));
                double azim = atan2(-localToFrame(0, 1), localToFrame(0, 0));

                osg::Quat finalTetherRotation;
                finalTetherRotation.makeRotate(-azim, 0.0, 0.0, 1.0);
                _tetherRotation.slerp(t, _tetherRotationVP0, finalTetherRotation);
            }

            // Track all rotations
            else if (_settings->getTetherMode() == TETHER_CENTER_AND_ROTATION)
            {
                osg::Quat finalTetherRotation;
                finalTetherRotation = L2W.getRotate() * _centerRotation.inverse();
                _tetherRotation.slerp(t, _tetherRotationVP0, finalTetherRotation);
            }
        }
#endif

        _state.lastTetherMode = _settings->tetherMode;
    }
}