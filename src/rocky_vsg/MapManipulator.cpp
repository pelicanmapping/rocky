/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MapManipulator.h"
#include "MapNode.h"
#include "Utils.h"

#include <rocky/Units.h>
#include <rocky_vsg/TerrainNode.h>

#include <vsg/io/Options.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/utils/LineSegmentIntersector.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/ui/KeyEvent.h>
#include <vsg/ui/ScrollWheelEvent.h>

using namespace ROCKY_NAMESPACE;

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
        return (double)temp.count() * 0.000000001;
    }

    template<class DMAT4>
    DMAT4 extract_rotation(const DMAT4& m) {
        DMAT4 r = m;
        r[3][0] = r[3][1] = r[3][2] = 0.0;
        return r;
    }

#if 0
    vsg::dmat4 computeLocalToWorld(vsg::Node* node) {
        vsg::dmat4 m;
        if ( node ) {
            vsg::NodePathList nodePaths = node->getParentalNodePaths();
            if ( nodePaths.size() > 0 ) {
                vsg::NodePath p;
                unsigned start = 0;
                for(unsigned i=0; i<nodePaths[0].size(); ++i) {
                    if (dynamic_cast<MapNode*>(nodePaths[0][i]) != 0L) {
                        start = i;
                        break;
                    }
                }
                for(unsigned i=start; i<nodePaths[0].size(); ++i)
                    p.push_back(nodePaths[0][i]);
                
                m = osg::computeLocalToWorld(p);
                //m = osg::computeLocalToWorld( nodePaths[0] );
            }
            else {
                osg::Transform* t = dynamic_cast<osg::Transform*>(node);
                if ( t ) {
                    t->computeLocalToWorldMatrix( m, 0L );
                }
            }
        }
        return m;
    }

    vsg::dvec3 computeWorld(vsg::Node* node) {
        return node ? vsg::dvec3(0,0,0) * computeLocalToWorld(node) : vsg::dvec3(0,0,0);
    }
#endif

    dvec3 computeWorld(vsg::ref_ptr<vsg::Node> node)
    {
        // TODO
        return dvec3(0, 0, 0);
    }

    double normalizeAzimRad( double input )
    {
        if(fabs(input) > 2*M_PI)
            input = fmod(input,2*M_PI);
        if( input < -M_PI ) input += M_PI*2.0;
        if( input > M_PI ) input -= M_PI*2.0;
        return input;
    }

#if 0
    // This replaces OSG's osg::computeLocalToWorld() function with one that
    // passes your own NodeVisitor to the Transform::computeLocalToWorldMatrix()
    // method. (We cannot subclass OSG's visitor because it's private.) This
    // exists for users that have custom Transform subclasses that override
    // Transform::computeLocalToWorldMatrix and need access to the NodeVisitor.
    struct ComputeLocalToWorld : vsg::NodeVisitor
    {
        vsg::dmat4 _matrix;
        vsg::NodeVisitor* _nv;
        ComputeLocalToWorld(vsg::NodeVisitor* nv) : _nv(nv) { }
        void accumulate(const vsg::NodePath& path)
        {
            if (path.empty()) return;
            unsigned j = path.size();
            for (vsg::NodePath::const_reverse_iterator i = path.rbegin();
                i != path.rend();
                ++i, --j)
            {
                const osg::Camera* cam = dynamic_cast<const osg::Camera*>(*i);
                if (cam)
                {
                    if( cam->getReferenceFrame() != osg::Transform::RELATIVE_RF || cam->getParents().empty())
                        break;
                }
                else if (dynamic_cast<MapNode*>(*i))
                {
                    break;
                }
            }
            for (; j<path.size(); ++j)
            {
                const_cast<vsg::Node*>(path[j])->accept(*this);
            }
        }
        void apply(osg::Transform& transform)
        {
            transform.computeLocalToWorldMatrix(_matrix, _nv);
        }
    };
#endif
}

#if 0

//------------------------------------------------------------------------
namespace
{
    // Callback that notifies the manipulator whenever the terrain changes
    // around its center point.
    struct ManipTerrainCallback : public TerrainCallback
    {
        ManipTerrainCallback(MapManipulator* manip) : _manip(manip) { }
        void onTileUpdate(const TileKey& key, vsg::Node* graph, TerrainCallbackContext& context)
        {
            osg::ref_ptr<MapManipulator> safe;
            if ( _manip.lock(safe) )
            {
                safe->handleTileUpdate(key, graph, context);
            }
        }
        osg::observer_ptr<MapManipulator> _manip;
    };
}
#endif


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


MapManipulator::Settings::Settings() :
    _single_axis_rotation(false),
    _lock_azim_while_panning(true),
    _mouse_sens(1.0),
    _touch_sens(0.005),
    _keyboard_sens(1.0),
    _scroll_sens(1.0),
    _min_pitch(-89.9),
    _max_pitch(-1.0),
    _max_x_offset(0.0),
    _max_y_offset(0.0),
    _min_distance(1.0),
    _max_distance(DBL_MAX),
    _tether_mode(TETHER_CENTER),
    _arc_viewpoints(true),
    _auto_vp_duration(false),
    _min_vp_duration_s(3.0),
    _max_vp_duration_s(8.0),
    _orthoTracksPerspective(true),
    _terrainAvoidanceEnabled(false),
    _terrainAvoidanceMinDistance(1.0),
    _throwingEnabled(false),
    _throwDecayRate(0.05),
    _zoomToMouse(true)
{
    //NOP
}

MapManipulator::Settings::Settings(const MapManipulator::Settings& rhs) :
    _bindings(rhs._bindings),
    _single_axis_rotation(rhs._single_axis_rotation),
    _lock_azim_while_panning(rhs._lock_azim_while_panning),
    _mouse_sens(rhs._mouse_sens),
    _touch_sens(rhs._touch_sens),
    _keyboard_sens(rhs._keyboard_sens),
    _scroll_sens(rhs._scroll_sens),
    _min_pitch(rhs._min_pitch),
    _max_pitch(rhs._max_pitch),
    _max_x_offset(rhs._max_x_offset),
    _max_y_offset(rhs._max_y_offset),
    _min_distance(rhs._min_distance),
    _max_distance(rhs._max_distance),
    _tether_mode(rhs._tether_mode),
    _arc_viewpoints(rhs._arc_viewpoints),
    _auto_vp_duration(rhs._auto_vp_duration),
    _min_vp_duration_s(rhs._min_vp_duration_s),
    _max_vp_duration_s(rhs._max_vp_duration_s),
    _orthoTracksPerspective(rhs._orthoTracksPerspective),
    _breakTetherActions(rhs._breakTetherActions),
    _terrainAvoidanceEnabled(rhs._terrainAvoidanceEnabled),
    _terrainAvoidanceMinDistance(rhs._terrainAvoidanceMinDistance),
    _throwingEnabled(rhs._throwingEnabled),
    _throwDecayRate(rhs._throwDecayRate),
    _zoomToMouse(rhs._zoomToMouse)
{
    //NOP
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
    //else if (HASMODKEY(m, osgGA::GUIEventAdapter::MODKEY_HYPER))
    //{
    //    expandSpec(InputSpec(e, i, m & ~osgGA::GUIEventAdapter::MODKEY_LEFT_HYPER), output);
    //    expandSpec(InputSpec(e, i, m & ~osgGA::GUIEventAdapter::MODKEY_RIGHT_HYPER), output);
    //}

    //Always add the input so if we are dealing with a windowing system like QT that just sends MODKEY_CTRL it will still work.
    output.push_back(input);
}

void
MapManipulator::Settings::bind(const InputSpec& spec, const Action& action)
{
    InputSpecs specs;
    expandSpec(spec, specs);
    for (InputSpecs::const_iterator i = specs.begin(); i != specs.end(); i++)
    {
        _bindings[*i] = action;
    }
}

void
MapManipulator::Settings::bindMouse(
    ActionType actionType,
    int button_mask, int modkey_mask,
    const ActionOptions& options)
{
    bind(
        InputSpec(EVENT_MOUSE_DRAG, button_mask, modkey_mask),
        Action(actionType, options));
}

void
MapManipulator::Settings::bindMouseClick(
    ActionType action,
    int button_mask, int modkey_mask,
    const ActionOptions& options)
{
    bind(
        InputSpec(EVENT_MOUSE_CLICK, button_mask, modkey_mask),
        Action(action, options));
}

void
MapManipulator::Settings::bindMouseDoubleClick(
    ActionType action,
    int button_mask, int modkey_mask,
    const ActionOptions& options)
{
    bind(
        InputSpec(EVENT_MOUSE_DOUBLE_CLICK, button_mask, modkey_mask),
        Action(action, options));
}

void
MapManipulator::Settings::bindKey(
    ActionType action,
    int key, int modkey_mask,
    const ActionOptions& options)
{
    bind(
        InputSpec(EVENT_KEY_DOWN, key, modkey_mask),
        Action(action, options));
}

void
MapManipulator::Settings::bindScroll(
    ActionType action, int scrolling_direction,
    int modkey_mask, const ActionOptions& options)
{
    bind(
        InputSpec(EVENT_SCROLL, scrolling_direction, modkey_mask),
        Action(action, options));
}


void
MapManipulator::Settings::bindPinch(
    ActionType action, const ActionOptions& options)
{
    bind(
        InputSpec(MapManipulator::EVENT_MULTI_PINCH, 0, 0),
        Action(action, options));
}

void
MapManipulator::Settings::bindTwist(
    ActionType action, const ActionOptions& options)
{
    bind(
        InputSpec(MapManipulator::EVENT_MULTI_TWIST, 0, 0),
        Action(action, options));
}

void
MapManipulator::Settings::bindMultiDrag(
    ActionType action, const ActionOptions& options)
{
    bind(
        InputSpec(MapManipulator::EVENT_MULTI_DRAG, 0, 0),
        Action(action, options));
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

void
MapManipulator::Settings::setMinMaxPitch( double min_pitch, double max_pitch )
{
    _min_pitch = clamp( min_pitch, -89.9, 89.0 );
    _max_pitch = clamp( max_pitch, min_pitch, 89.0 );
    dirty();
}

void
MapManipulator::Settings::setMaxOffset(double max_x_offset, double max_y_offset)
{
    _max_x_offset = std::max(max_x_offset, 0.0);
    _max_y_offset = std::max(max_y_offset, 0.0);
    dirty();
}

void
MapManipulator::Settings::setMinMaxDistance( double min_distance, double max_distance)
{
    _min_distance = min_distance;
    _max_distance = max_distance;
    dirty();
}

void
MapManipulator::Settings::setArcViewpointTransitions( bool value )
{
    _arc_viewpoints = value;
    dirty();
}

void
MapManipulator::Settings::setAutoViewpointDurationEnabled( bool value )
{
    _auto_vp_duration = value;
    dirty();
}

void
MapManipulator::Settings::setAutoViewpointDurationLimits(double minSeconds, double maxSeconds)
{
    _min_vp_duration_s = std::max(minSeconds, 0.0);
    _max_vp_duration_s = std::max(maxSeconds, _min_vp_duration_s);
    dirty();
}

/************************************************************************/

MapManipulator::MapManipulator(vsg::ref_ptr<MapNode> mapNode, vsg::ref_ptr<vsg::Camera> camera) :
    Inherit(),
    _mapNode(mapNode),
    _camera(camera),
    _lastAction(ACTION_NULL)
{
    if (mapNode.valid())
        _worldSRS = mapNode->worldSRS();
        //_srs = mapNode->mapSRS();

    reinitialize();

    configureDefaultSettings();


    // compute the bounds of the scene graph to help position camera
    //auto center = vsg::dvec3(0, 0, 0); // (cb.bounds.min + cb.bounds.max) * 0.5;
    //auto radius = _worldSRS.ellipsoid().semiMajorAxis();
    //setCenter(vsg::dvec3(radius, 0, 0));
    //setDistance(radius * 3.5);
    home();

    //if (_settings)
    //    _lastTetherMode = _settings->getTetherMode();
}

MapManipulator::~MapManipulator()
{
    //vsg::ref_ptr<MapNode> mapNode = _mapNode;
    //if (mapNode.valid() && _terrainCallback && mapNode->getTerrain())
    //{
    //    mapNode->getTerrain()->removeTerrainCallback( _terrainCallback.get() );
    //}
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
    options.add(OPTION_SCALE_Y, 5.0);

    // zoom as you hold the right button:
    _settings->bindMouse(ACTION_ZOOM, MOUSE_RIGHT_BUTTON, 0L, options);
    _settings->bindMouse(ACTION_ZOOM, MOUSE_RIGHT_BUTTON, vsg::MODKEY_Control, options);

    options.add(OPTION_SCALE_X, 9.0);
    options.add(OPTION_SCALE_Y, 9.0);

    _settings->bindMouse(ACTION_PAN, MOUSE_LEFT_BUTTON);
    _settings->bindMouse(ACTION_PAN, MOUSE_LEFT_BUTTON, vsg::MODKEY_Control, options);

    // rotate with either the middle button or the left+right buttons:
    _settings->bindMouse(ACTION_ROTATE, MOUSE_MIDDLE_BUTTON);
    _settings->bindMouse(ACTION_ROTATE, MOUSE_LEFT_BUTTON | MOUSE_RIGHT_BUTTON);
    _settings->bindMouse(ACTION_ROTATE, MOUSE_MIDDLE_BUTTON, vsg::MODKEY_Control, options);
    _settings->bindMouse(ACTION_ROTATE, MOUSE_LEFT_BUTTON | MOUSE_RIGHT_BUTTON, vsg::MODKEY_Control, options);

    options.add(OPTION_SCALE_X, 4.0);
    options.add(OPTION_SCALE_Y, 4.0);

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

    //_settings->setThrowingEnabled( false );
    _settings->setLockAzimuthWhilePanning(true);

    _settings->setZoomToMouse(false);
}

void
MapManipulator::applySettings(shared_ptr<Settings> settings)
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

    //flushMouseEventStack();

    // apply new pitch restrictions
    double old_pitch_rad;
    getEulerAngles(_state.localRotation, nullptr, &old_pitch_rad);

    double old_pitch_deg = rad2deg(old_pitch_rad);
    double new_pitch_deg = clamp(old_pitch_deg, _settings->getMinPitch(), _settings->getMaxPitch());

    setDistance(_state.distance);

#if 0
    if (!equiv(new_pitch_deg, old_pitch_deg))
    {
        Viewpoint vp = getViewpoint();
        vp.pitch->set(new_pitch_deg, Units::DEGREES);
        setViewpoint(vp);
    }
#endif
}

shared_ptr<MapManipulator::Settings>
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
    _continuous = false;
    _lastAction = ACTION_NULL;
    clearEvents();
}

#if 0
void
MapManipulator::handleTileUpdate(const TileKey& key, vsg::Node* graph, TerrainCallbackContext& context)
{
    // Only do collision avoidance if it's enabled, we're not tethering and
    // we're not in the middle of setting a viewpoint.
    if (getSettings()->getTerrainAvoidanceEnabled() &&
        !isTethering() &&
        !isSettingViewpoint() )
    {
        const GeoPoint& pt = centerMap();
        if ( key.extent().contains(pt.x(), pt.y()) )
        {
            recalculateCenterFromLookVector();
            collisionDetect();
        }
    }
}
#endif

bool
MapManipulator::createLocalCoordFrame(
    const vsg::dvec3& worldPos,
    vsg::dmat4& out_frame) const
{
    if (_worldSRS.valid())
    {        
        out_frame = to_vsg(_worldSRS.localToWorldMatrix(to_glm(worldPos)));
    }
    return _worldSRS.valid();
}


void
MapManipulator::setCenter(const vsg::dvec3& worldPos)
{
    _state.center = worldPos;

    dmat4 m = _worldSRS.localToWorldMatrix(to_glm(worldPos));

    // remove the translation component
    _state.centerRotation = to_vsg(m);
    _state.centerRotation[3][0] = 0;
    _state.centerRotation[3][1] = 0;
    _state.centerRotation[3][2] = 0;
}

#if 0
void
MapManipulator::setNode(vsg::Node* node)
{
    // you can only set the node if it has not already been set, OR if you are setting
    // it to NULL. (So to change it, you must first set it to NULL.) This is to prevent
    // OSG from overwriting the node after you have already set on manually.
    if ( node == 0L || !_node.valid() )
    {
        _node     = node;
        _mapNode = 0L;
        _srs     = 0L;

        reinitialize();
        established();
    }
}

vsg::Node*
MapManipulator::getNode()
{
    return _node;
}
#endif


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

#if 0
Viewpoint
MapManipulator::getViewpoint() const
{
    Viewpoint vp;

#if 0
    // Tethering? Use the tether viewpoint.
    if ( isTethering() && _setVP1.isSet() )
    {
        vp = _setVP1.get();

        if (vp.node)
            vp.focalPoint->fromWorld(_srs.get(), computeWorld(vp.node));
        else
            vp.focalPoint.unset();
    }
#endif

    // Transitioning? Capture the last calculated intermediate position.
    //else 
    if (isSettingViewpoint())
    {
        vp.point->fromWorld(_srs, _center);
    }

    // If we are stationary:
    else
    {
        vp.point->fromWorld(_srs, _center);
    }

    // Always update the local offsets.
    double localAzim, localPitch;
    getEulerAngles( _rotation, &localAzim, &localPitch );

    vp.heading = Angle(localAzim, Units::RADIANS).to(Units::DEGREES);
    vp.pitch = Angle(localPitch, Units::RADIANS).to(Units::DEGREES);
    vp.range->set(_distance, Units::METERS);

    if ( _posOffset.x != 0.0 || _posOffset.y != 0.0 || _posOffset.z != 0.0 )
    {
        vp.positionOffset = to_glm(_posOffset);
    }

    return vp;
}

void
MapManipulator::setViewpoint(const Viewpoint& vp, double duration_seconds)
{
    // If the manip is not set up, save the viewpoint for later.
    if ( !established() )
    {
        _pendingViewpoint = vp;
        _pendingViewpointDuration.set(duration_seconds, Units::SECONDS);
    }

    else
    {
#if 0
        // Save any existing tether node so we can properly invoke the callback.
        osg::ref_ptr<vsg::Node> oldEndNode;
        if ( isTethering() && _tetherCallback.valid() )
        {
            oldEndNode = _setVP1->getNode();
        }
#endif

        // starting viewpoint; all fields will be set:
        _setVP0 = getViewpoint();

        // ending viewpoint
        _setVP1 = vp;

        // Reset the tethering offset quat.
        _tetherRotationVP0 = _tetherRotation;
        _tetherRotationVP1 = vsg::dquat();

        // Fill in any missing end-point data with defaults matching the current camera setup.
        // Then all fields are guaranteed to contain usable data during transition.
        double defPitch, defAzim;
        getEulerAngles( _rotation, &defAzim, &defPitch );

        if ( !_setVP1->heading.isSet() )
            _setVP1->heading = Angle(defAzim, Units::RADIANS);

        if ( !_setVP1->pitch.isSet() )
            _setVP1->pitch = Angle(defPitch, Units::RADIANS);

        if ( !_setVP1->range.isSet() )
            _setVP1->range = Distance(_distance, Units::METERS);

#if 0
        if ( !_setVP1->nodeIsSet() && !_setVP1->focalPoint().isSet() )
        {
            osg::ref_ptr<vsg::Node> vpNode = _setVP0->getNode();
            if (vpNode.valid())
                _setVP1->setNode(vpNode.get());
            else
                _setVP1->focalPoint() = _setVP0->focalPoint().get();
        }
#else
        _setVP1->point = _setVP0->point;
#endif

        _setVPDuration.set(std::max(duration_seconds, 0.0), Units::SECONDS);

        ROCKY_DEBUG << LC << "setViewpoint:\n"
            << "    from " << _setVP0->toString() << "\n"
            << "    to   " << _setVP1->toString() << "\n";

#if 0
        // access the new tether node if it exists:
        osg::ref_ptr<vsg::Node> endNode = _setVP1->getNode();
#endif

        // Timed transition, we need to calculate some things:
        if ( duration_seconds > 0.0 )
        {
            // Start point is the current manipulator center:
            vsg::dvec3 startWorld;
            vsg::ref_ptr<vsg::Node> startNode = {}; // _setVP0->getNode();
            startWorld = (_center); // startNode ? computeWorld(startNode) : _center;

            _setVPStartTime.unset();

            // End point is the world coordinates of the target viewpoint:
            vsg::dvec3 endWorld;
            //if ( endNode )
            //    endWorld = computeWorld(endNode.get());
            //else
                _setVP1->point->transform(_srs).toWorld(endWorld);

            // calculate an acceleration factor based on the Z differential.
            _setVPArcHeight = 0.0;
            double range0 = _setVP0->range->as(Units::METERS);
            double range1 = _setVP1->range->as(Units::METERS);

            double pitch0 = _setVP0->pitch->as(Units::RADIANS);
            double pitch1 = _setVP1->pitch->as(Units::RADIANS);

            double h0 = range0 * sin( -pitch0 );
            double h1 = range1 * sin( -pitch1 );
            double dh = (h1 - h0);

            // calculate the total distance the focal point will travel and derive an arc height:
            double de = length(endWorld - startWorld);

            // maximum height during viewpoint transition
            if ( _settings->getArcViewpointTransitions() )
            {
                _setVPArcHeight = std::max( de - fabs(dh), 0.0 );
            }

            // calculate acceleration coefficients
            if ( _setVPArcHeight > 0.0 )
            {
                // if we're arcing, we need separate coefficients for the up and down stages
                double h_apex = 2.0*(h0+h1) + _setVPArcHeight;
                double dh2_up = fabs(h_apex - h0)/100000.0;
                _setVPAccel = log10( dh2_up );
                double dh2_down = fabs(h_apex - h1)/100000.0;
                _setVPAccel2 = -log10( dh2_down );
            }
            else
            {
                // on arc => simple unidirectional acceleration:
                double dh2 = (h1 - h0)/100000.0;
                _setVPAccel = fabs(dh2) <= 1.0? 0.0 : dh2 > 0.0? log10( dh2 ) : -log10( -dh2 );
                if ( fabs( _setVPAccel ) < 1.0 ) _setVPAccel = 0.0;
            }

            // Adjust the duration if necessary.
            if ( _settings->getAutoViewpointDurationEnabled() )
            {
                double maxDistance = _srs.ellipsoid().semiMajorAxis();
                double ratio = clamp(de / maxDistance, 0.0, 1.0);
                ratio = accelerationInterp(ratio, -4.5);
                double minDur, maxDur;
                _settings->getAutoViewpointDurationLimits(minDur, maxDur);
                _setVPDuration.set(minDur + ratio * (maxDur - minDur), Units::SECONDS);
            }
        }

        else
        {
            // Immediate transition? Just do it now.
            _setVPStartTime->set( _time_s_now, Units::SECONDS );
            setViewpointFrame( _time_s_now );
        }

#if 0
        // Fire a tether callback if required.
        if ( _tetherCallback.valid() )
        {
            // starting a tether to a NEW node:
            if ( isTethering() && oldEndNode.get() != endNode.get() )
                (*_tetherCallback)( endNode.get() );

            // breaking a tether:
            else if ( !isTethering() && oldEndNode.valid() )
                (*_tetherCallback)( 0L );
        }
#endif
    }

    // reset other global state flags.
    _thrown      = false;
    _task._type = TASK_NONE;
}

// returns "t" [0..1], the interpolation coefficient.
double
MapManipulator::setViewpointFrame(double time_s)
{
    if ( !_setVPStartTime.isSet() )
    {
        _setVPStartTime->set( time_s, Units::SECONDS );
        return 0.0;
    }
    else
    {
        // Start point is the current manipulator center:
        dvec3 startWorld;
        vsg::ref_ptr<vsg::Node> startNode = {}; // _setVP0->getNode();
        if (startNode)
            startWorld = computeWorld(startNode);
        else
            _setVP0->point->transform(_srs).toWorld(startWorld);

        // End point is the world coordinates of the target viewpoint:
        dvec3 endWorld;
        vsg::ref_ptr<vsg::Node> endNode = { }; // _setVP1->getNode();
        if (endNode)
            endWorld = computeWorld(endNode);
        else
            _setVP1->point->transform(_srs).toWorld(endWorld);

        // Remaining time is the full duration minus the time since initiation:
        double elapsed = time_s - _setVPStartTime->as(Units::SECONDS);
        double duration = _setVPDuration.as(Units::SECONDS);
        double t = std::min(1.0, duration > 0.0 ? elapsed/duration : 1.0);
        double tp = t;

        if ( _setVPArcHeight > 0.0 )
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
            tp = smoothStepInterp( tp );
        }
        else if ( t > 0.0 )
        {
            tp = smoothStepInterp( tp );
        }

        vsg::dvec3 newCenter =
            _srs.isGeographic()
            ? nlerp(startWorld, endWorld, tp)
            : lerp(startWorld, endWorld, tp);

        // Calculate the delta-heading, and make sure we are going in the shortest direction:
        Angle d_azim = _setVP1->heading.get() - _setVP0->heading.get();
        if ( d_azim.as(Units::RADIANS) > M_PI )
            d_azim = d_azim - Angle(2.0*M_PI, Units::RADIANS);
        else if ( d_azim.as(Units::RADIANS) < -M_PI )
            d_azim = d_azim + Angle(2.0*M_PI, Units::RADIANS);
        double newAzim = _setVP0->heading->as(Units::RADIANS) + tp*d_azim.as(Units::RADIANS);

        // Calculate the new pitch:
        Angle d_pitch = _setVP1->pitch.get() - _setVP0->pitch.get();
        double newPitch = _setVP0->pitch->as(Units::RADIANS) + tp*d_pitch.as(Units::RADIANS);

        // Calculate the new range:
        Distance d_range = _setVP1->range.get() - _setVP0->range.get();
        double newRange =
            _setVP0->range->as(Units::METERS) +
            d_range.as(Units::METERS)*tp + sin(M_PI*tp)*_setVPArcHeight;

        // Calculate the offsets
        vsg::dvec3 offset0 = to_vsg(_setVP0->positionOffset.getOrUse(dvec3(0, 0, 0)));
        vsg::dvec3 offset1 = to_vsg(_setVP1->positionOffset.getOrUse(dvec3(0, 0, 0)));
        vsg::dvec3 newOffset = offset0 + (offset1-offset0)*tp;

        // Activate.
        setLookAt( newCenter, newAzim, newPitch, newRange, newOffset );

        // interpolate tether rotation:
        _tetherRotation = vsg::mix(_tetherRotationVP0, _tetherRotationVP1, tp);
        //_tetherRotation.slerp(tp, _tetherRotationVP0, _tetherRotationVP1);

        // At t=1 the transition is complete.
        if ( t >= 1.0 )
        {
            _setVP0.unset();

            // If this was a transition into a tether, keep the endpoint around so we can
            // continue tracking it.
            if ( !isTethering() )
            {
                _setVP1.unset();
            }
        }

        return tp;
    }
}
#endif


#if 0
void
MapManipulator::setLookAt(
    const vsg::dvec3& center,
    double azim,
    double pitch,
    double range,
    const vsg::dvec3& posOffset)
{
    setCenter(center);
    setDistance(range);

    //_previousUp = getUpVector(_centerReferenceFrame);
    //_centerRotation = computeCenterRotation(center);

    _localPositionOffset = posOffset;

    azim = normalizeAzimRad(azim);

    pitch = clamp(
        pitch,
        deg2rad(_settings->getMinPitch()),
        deg2rad(_settings->getMaxPitch()));

    _localRotation = getQuaternion(azim, pitch);
}

void
MapManipulator::resetLookAt()
{
    double pitch;
    getEulerAngles(_localRotation, 0L, &pitch );

    double maxPitch = deg2rad(-10.0);
    if ( pitch > maxPitch )
        rotate( 0.0, -(pitch-maxPitch) );

    vsg::dvec3 eye = getTrans(getWorldMatrix());

    // calculate the center point in front of the eye. The reference frame here
    // is the view plane of the camera.
    //vsg::dmat4 m = quat_to_mat4(_localRotation * _centerRotation);
    //recalculateCenter(m);

    double newDistance = length(eye - _center);
    setDistance(newDistance);

    _localPositionOffset.set(0, 0, 0);
    _viewOffset.set(0, 0);

    //_tetherRotation = vsg::dquat();
    //_tetherRotationVP0 = vsg::dquat();
    //_tetherRotationVP1 = vsg::dquat();
}
#endif

#if 0
bool
MapManipulator::isSettingViewpoint() const
{
    return _setVP0.isSet() && _setVP1.isSet();
}

void
MapManipulator::clearViewpoint()
{
    bool breakingTether = isTethering();

    // Cancel any ongoing transition or tethering:
    _setVP0.unset();
    _setVP1.unset();

    // Restore the matrix values in a neutral state.
    recalculateCenterFromLookVector();

#if 0
    // Fire the callback to indicate a tethering break.
    if ( _tetherCallback.valid() && breakingTether )
        (*_tetherCallback)( 0L );
#endif
}

bool
MapManipulator::isTethering() const
{
    // True if setViewpoint() was called and the viewpoint has a node.
    //return _setVP1.isSet() && _setVP1->nodeIsSet();
    return false;
}
#endif

#if 0
void MapManipulator::collisionDetect()
{
    if (!getSettings()->getTerrainAvoidanceEnabled() || !_srs )
    {
        return;
    }
    // The camera has changed, so make sure we aren't under the ground.

    vsg::dvec3 eye = getTrans(getWorldMatrix());
    vsg::dmat4 eyeCoordFrame;
    createLocalCoordFrame(eye, eyeCoordFrame);
    vsg::dvec3 eyeUp = getUpVector(eyeCoordFrame);

    // Try to intersect the terrain with a vector going straight up and down.
    double r = std::min(_srs.ellipsoid().semiMajorAxis(), _srs.ellipsoid().semiMinorAxis());
    vsg::dvec3 ip, normal;

    if (intersect(eye + eyeUp * r, eye - eyeUp * r, ip, normal))
    {
        double eps = _settings->getTerrainAvoidanceMinimumDistance();
        // Now determine if the point is above the ground or not
        vsg::dvec3 v0 = vsg::normalize(eyeUp);
        //v0.normalize();
        vsg::dvec3 v1 = vsg::normalize(eye - (ip + eyeUp * eps));
        //v1.normalize();

        // save rotation so we can restore it later - the setraw method
        // may alter it and we don't want that.
        vsg::dquat rotation = _localRotation;

        //vsg::dvec3 adjVector = normal;
        vsg::dvec3 adjVector = eyeUp;
        if (vsg::dot(v0, v1) <= 0)
        {
            setByLookAtRaw(ip + adjVector * eps, _center, eyeUp);
            _localRotation = rotation;
        }

        //ROCKY_INFO << "hit at " << ip.x() << ", " << ip.y() << ", " << ip.z() << "\n";
    }
}
#endif

vsg::ref_ptr<MapNode>
MapManipulator::getMapNode() const
{
    vsg::ref_ptr<MapNode> safe = _mapNode;
    return safe;
}

bool
MapManipulator::intersect(
    const vsg::dvec3& start,
    const vsg::dvec3& end,
    vsg::dvec3& out_intersection) const
{
    vsg::ref_ptr<MapNode> mapNode = _mapNode;
    if (mapNode)
    {
        vsg::LineSegmentIntersector lsi(start, end);

        mapNode->terrainNode()->accept(lsi);

        if (!lsi.intersections.empty())
        {
            if (lsi.intersections.size() > 1)
            {
                // sort from closest to farthest
                std::sort(lsi.intersections.begin(), lsi.intersections.end(),
                    [](const vsg::ref_ptr<vsg::LineSegmentIntersector::Intersection>& lhs,
                        const vsg::ref_ptr<vsg::LineSegmentIntersector::Intersection>& rhs)
                    {
                        return lhs->ratio < rhs->ratio;
                    });
            }

            out_intersection = lsi.intersections.front()->worldIntersection;
            //std::cout << out_intersection.x << ", " << out_intersection.y << ", " << out_intersection.z << std::endl;
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
    _continuous = false;
    _keyPress.clear();
    _buttonPress.clear();
    _buttonRelease.clear();
    _task.reset();
    _dirty = true;
}

void
MapManipulator::apply(vsg::KeyPressEvent& keyPress)
{
    _keyPress = keyPress;

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
    //std::cout << "ButtonPressEvent" << std::endl;

    // simply record the button press event.
    clearEvents();

    _buttonPress = buttonPress;

    buttonPress.handled = true;
}

void
MapManipulator::apply(vsg::ButtonReleaseEvent& buttonRelease)
{
    //std::cout << "ButtonReleaseEvent" << std::endl;

    _buttonRelease = buttonRelease;

    if (isMouseClick())
    {
        _lastAction = _settings->getAction(
            EVENT_MOUSE_CLICK,
            _buttonPress->button,
            _buttonPress->mask);

        if (handlePointAction(_lastAction, _buttonRelease->x, _buttonRelease->y, buttonRelease.time))
            _dirty = true;
    }

    clearEvents();

    buttonRelease.handled = true;
}

void
MapManipulator::apply(vsg::MoveEvent& moveEvent)
{
    //std::cout << "MoveEvent, mask = " << moveEvent.mask << std::endl;

    _previousMove = _currentMove;
    _currentMove = moveEvent;

    if (moveEvent.mask != 0) // if a button is pressed
    {
        vsg::ref_ptr<vsg::Window> window = moveEvent.window;

        _lastAction = _settings->getAction(
            EVENT_MOUSE_DRAG,
            moveEvent.mask, // button mask
            _keyPress.isSet() ? _keyPress->keyModifier : 0);

        bool wasContinuous = _continuous;
        _continuous = _lastAction.getBoolOption(OPTION_CONTINUOUS, false);

        if (handleMouseAction(_lastAction, moveEvent.time))
            _dirty = true;

        if (_continuous && !wasContinuous)
        {
            _continuousAction = _lastAction;
            _last_continuous_action_time = moveEvent.time;
        }

        if (_continuous)
            _dirty = true;

        _thrown = false;
        moveEvent.handled = true;
    }

    else
    {
        // button was released outside the frame
        clearEvents();
    }
}

void
MapManipulator::apply(vsg::ScrollWheelEvent& scrollEvent)
{
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
        _keyPress.isSet() ? _keyPress->keyModifier : 0);

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
    //TEST_OUT << "FrameEvent-------------------------- " << frame.time.time_since_epoch().count() << std::endl;
    if (_continuous)
    {
        handleContinuousAction(_continuousAction, frame.time);
    }
    else
    {
        _continuousDelta.set(0.0, 0.0);
    }

    serviceTask(frame.time);

    _viewMatrix =
        vsg::translate(_state.center) *
        _state.centerRotation *
        vsg::rotate(_state.localRotation) *
        vsg::translate(0.0, 0.0, _state.distance);

    auto lookat = _camera->viewMatrix.cast<vsg::LookAt>();
    if (!lookat)
    {
        lookat = vsg::LookAt::create();
        _camera->viewMatrix = lookat;
    }

    lookat->set(_viewMatrix);

    _dirty = false;
}

bool
MapManipulator::serviceTask(vsg::time_point now)
{
    if (_task._type != TASK_NONE)
    {
        auto dt = to_seconds(now - _task._time_last_service);
        if (dt > 0.0)
        {
            // cap the DT so we don't exceed the expected delta.
            dt = std::min(dt, _task._duration_s);

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
            _task._time_last_service = now;

            if (_task._duration_s <= 0.0)
            {
                _task._type = TASK_NONE;
            }
        }
    }

    // returns true if the task is still running.
    return _task._type != TASK_NONE;
}


bool
MapManipulator::isMouseClick() const
{
    if (!_buttonPress.isSet() || !_buttonRelease.isSet())
        return false;

    static const float velocity = 0.1f;

    auto down = ndc(_buttonPress.value());
    auto up = ndc(_buttonRelease.value());

    float dx = up.x - down.x;
    float dy = up.y - down.y;
    float len = sqrtf(dx * dx + dy * dy);
    auto dtmillis = std::chrono::duration_cast<std::chrono::milliseconds>(_buttonRelease->time - _buttonPress->time);
    float dt = (float)dtmillis.count() * 0.001f;
    return (len < dt* velocity);
}

bool
MapManipulator::recalculateCenterFromLookVector()
{
    vsg::LookAt lookat;
    lookat.set(_camera->viewMatrix->inverse());
    auto look = vsg::normalize(lookat.center - lookat.eye);
    
    bool ok = false;
    vsg::dvec3 intersection;

    if (intersect(lookat.eye, look * _state.distance * 1.5, intersection))
    {
        ok = true;
    }

    // backup plan, intersect the ellipsoid or the ground plane
    else if (_worldSRS.isGeocentric())
    {
        dvec3 i;
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
#if 0
        setCenter(i);
#else
        // keep the existing center, but 
        double len = vsg::length(intersection);
        _state.center = vsg::normalize(_state.center) * len;
#endif
    }

    return ok;
}

void
MapManipulator::pan(double dx, double dy)
{
    // to pan, we need a focus point on the terrain:
    if ( !recalculateCenterFromLookVector() )
        return;

    double scale = -0.3 * _state.distance;

    // the view-space coordinate frame:
    auto lookat = _camera->viewMatrix->inverse();
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
    _viewOffset.x() = osg::clampBetween(_viewOffset.x(), -_settings->getMaxXOffset(), _settings->getMaxXOffset());
    _viewOffset.y() = osg::clampBetween(_viewOffset.y(), -_settings->getMaxYOffset(), _settings->getMaxYOffset());
    }
#endif

    //collisionDetect();
}

void
MapManipulator::rotate(double dx, double dy)
{
    // clamp the local pitch delta; never allow the pitch to hit -90.
    double minp = deg2rad(std::min(_settings->getMinPitch(), -89.9));
    double maxp = deg2rad(std::max(_settings->getMaxPitch(), 89.9));

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

    if (_settings->getZoomToMouse() == false)
    {
        recalculateCenterFromLookVector();
        double scale = 1.0f + dy;
        setDistance(_state.distance * scale);
        //collisionDetect();
        return;
    }

#if 0
    // Zoom to mouseish
    osgViewer::View* view = dynamic_cast<osgViewer::View*>(in_view);
    if ( !view )
        return;

    if (_ga_t0 == NULL)
        return;

    float x = _ga_t0->getX(), y = _ga_t0->getY();
    float local_x, local_y;

    const osg::Camera* camera = view->getCameraContainingPosition(x, y, local_x, local_y);
    if (!camera)
        camera = view->getCamera();

    if ( !camera )
        return;

    // reset the "remembered start location" if we're just starting a continuous zoom
    static vsg::dvec3 zero(0,0,0);
    if (_last_action._type != ACTION_ZOOM)
        _lastPointOnEarth = zero;

    vsg::dvec3 target;

    bool onEarth = true;
    if (_lastPointOnEarth != zero)
    {
        // Use the start location (for continuous zoom) 
        target = _lastPointOnEarth;
    }
    else
    {
        // Zoom just started; calculate a start location
        onEarth = screenToWorld(x, y, view, target);
    }

    if (onEarth)
    {
        _lastPointOnEarth = target;

        if (_srs.valid() && _srs.isGeographic())
        {
            // globe

            // Calcuate a rotation that we'll use to interpolate from our center point to the target
            osg::Quat rotCenterToTarget;
            rotCenterToTarget.makeRotate(_center, target);

            // Factor by which to scale the distance:
            double scale = 1.0f + dy;
            double newDistance = _distance*scale;
            double delta = _distance - newDistance;
            double ratio = delta/_distance;

            // xform target point into the current focal point's local frame,
            // and adjust the zoom ratio to account for the difference in 
            // target distance based on the earth's curvature...approximately!
            vsg::dvec3 targetInLocalFrame = _centerRotation.conj()*target;
            double crRatio = _center.length() / targetInLocalFrame.z();
            ratio *= crRatio;

            // Interpolate a new focal point:
            osg::Quat rot;
            rot.slerp(ratio, osg::Quat(), rotCenterToTarget);
            setCenter(rot*_center);

            // recompute the local frame:
            _centerRotation = computeCenterRotation(_center);

            // and set the new zoomed distance.
            setDistance(newDistance);

            collisionDetect();
        }
        else
        {
            // projected map. This will a simple linear interpolation
            // of the eyepoint along the path between the eye and the target.
            vsg::dvec3 eye, at, up;
            getWorldInverseMatrix().getLookAt(eye, at, up);

            vsg::dvec3 eyeToTargetVec = target-eye;
            eyeToTargetVec.normalize();

            double scale = 1.0f + dy;
            double newDistance = _distance*scale;
            double delta = _distance - newDistance;
            double ratio = delta/_distance;
            
            vsg::dvec3 newEye = eye + eyeToTargetVec*delta;

            setByLookAt(newEye, newEye+(at-eye), up);
        }
    }

    else
    {
        // if the user's mouse isn't over the earth, just zoom in to the center of the screen
        double scale = 1.0f + dy;
        setDistance( _distance * scale );
        collisionDetect();
    }
#endif
}


bool
MapManipulator::screenToWorld(float x, float y, vsg::dvec3& out_coords) const
{
    //osgViewer::View* view = dynamic_cast<osgViewer::View*>(theView);
    //if (!view || !view->getCamera())
    //    return false;

    auto mapNode = getMapNode();
    if (mapNode)
    {
        //osg::ref_ptr<MapNode> mapNode;
        //if (!_mapNode.lock(mapNode) || !mapNode->getTerrain())
        //    return false;

        NOT_YET_IMPLEMENTED("");
        return false;
        //return mapNode->terrainNode()->getWorldCoordsUnderMouse(window, x, y, out_coords);
    }
    else
        return false;

    //return mapNode->getTerrain()->getWorldCoordsUnderMouse(view, x, y, out_coords);
}


void
MapManipulator::setDistance(double distance)
{
    _state.distance = clamp(distance, _settings->getMinDistance(), _settings->getMaxDistance());
}

void
MapManipulator::handleMovementAction(
    const ActionType& type,
    vsg::dvec2 d,
    vsg::time_point time)
{
    switch (type)
    {
    case ACTION_PAN:
        pan(d.x, d.y);
        break;

    case ACTION_ROTATE:
        // in "single axis" mode, zero out one of the deltas.
        if (_continuous && _settings->getSingleAxisRotation())
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
    if (screenToWorld(mx, my, point))
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
MapManipulator::handleContinuousAction(const Action& action, vsg::time_point time)
{
    double t_factor = to_seconds(time - _last_continuous_action_time) * 60.0;
    _last_continuous_action_time = time;
    handleMovementAction(
        action._type,
        _continuousDelta * t_factor,
        time);
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
    vsg::time_point time)
{
    if (!_currentMove.has_value() || !_previousMove.has_value())
        return false;

    auto prev = ndc(_previousMove);
    auto curr = ndc(_currentMove);

    vsg::dvec2 delta(
        curr.x - prev.x,
        -(curr.y - prev.y));

    // return if there is no movement.
    if (delta.x == 0.0 && delta.y == 0.0)
        return false;

    // here we adjust for action scale, global sensitivy
    delta *= _settings->getMouseSensitivity();

    applyOptionsToDeltas(action, delta);

    // in "continuous" mode, we accumulate the deltas each frame - thus
    // the deltas act more like speeds.
    if (_continuous)
    {
        _continuousDelta += (delta * 0.01);
    }
    else
    {
        _delta = delta;
        handleMovementAction(action._type, delta, time);
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

    d.x *= _settings->getKeyboardSensitivity();
    d.y *= _settings->getKeyboardSensitivity();

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

    d.x *= scrollFactor * _settings->getScrollSensitivity();
    d.y *= scrollFactor * _settings->getScrollSensitivity();

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

    //if ( osgEarth::getNotifyLevel() > osg::INFO )
    //    dumpActionInfo( action, osg::DEBUG_INFO );

    //ROCKY_NOTICE << "action=" << action << ", dx=" << dx << ", dy=" << dy << std::endl;

    switch( action._type )
    {
#if 0
    case ACTION_HOME:
        if ( _homeViewpoint.isSet() )
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
MapManipulator::getCompositeEulerAngles(double* out_azim, double* out_pitch) const
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
std::pair<int32_t, int32_t>
MapManipulator::cameraRenderAreaCoordinates(const vsg::PointerEvent& pointerEvent) const
{
    if (!_windowOffsets.empty())
    {
        auto itr = _windowOffsets.find(pointerEvent.window);
        if (itr != _windowOffsets.end())
        {
            auto& offset = itr->second;
            return { pointerEvent.x + offset.x, pointerEvent.y + offset.y };
        }
    }
    return { pointerEvent.x, pointerEvent.y };
}

/// code adopted from VSG
bool
MapManipulator::withinRenderArea(const vsg::PointerEvent& pointerEvent) const
{
    auto renderArea = _camera->getRenderArea();
    auto [x, y] = cameraRenderAreaCoordinates(pointerEvent);

    return
        (x >= renderArea.offset.x && x < static_cast<int32_t>(renderArea.offset.x + renderArea.extent.width)) &&
        (y >= renderArea.offset.y && y < static_cast<int32_t>(renderArea.offset.y + renderArea.extent.height));
}


/// compute non dimensional window coordinate (-1,1) from event coords
/// code adopted from VSG
vsg::dvec2
MapManipulator::ndc(const vsg::PointerEvent& event) const
{
    auto renderArea = _camera->getRenderArea();
    auto [x, y] = cameraRenderAreaCoordinates(event);

    double aspectRatio = static_cast<double>(renderArea.extent.width) / static_cast<double>(renderArea.extent.height);
    vsg::dvec2 v(
        (renderArea.extent.width > 0) ? (static_cast<double>(x - renderArea.offset.x) / static_cast<double>(renderArea.extent.width) * 2.0 - 1.0) * aspectRatio : 0.0,
        (renderArea.extent.height > 0) ? static_cast<double>(y - renderArea.offset.y) / static_cast<double>(renderArea.extent.height) * 2.0 - 1.0 : 0.0);
    return v;
}