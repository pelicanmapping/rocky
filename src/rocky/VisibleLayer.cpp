/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "VisibleLayer.h"

using namespace rocky;

#define LC "[VisibleLayer] \"" << getName() << "\" "

#if 0
Config
VisibleLayer::Options::getConfig() const
{
    Config conf = Layer::Options::getConfig();
    conf.set( "visible", visible() );
    conf.set( "opacity", opacity() );
    conf.set( "mask", mask());
    conf.set( "min_range", minVisibleRange() );
    conf.set( "max_range", maxVisibleRange() );
    conf.set( "attenuation_range", attenuationRange() );
    conf.set( "blend", "interpolate", _blend, BLEND_INTERPOLATE );
    conf.set( "blend", "modulate", _blend, BLEND_MODULATE );
    return conf;
}

void
VisibleLayer::Options::fromConfig(const Config& conf)
{
    _visible.init( true );
    _opacity.init( 1.0f );
    _minVisibleRange.init( 0.0 );
    _maxVisibleRange.init( FLT_MAX );
    _attenuationRange.init(0.0f);
    _blend.init( BLEND_INTERPOLATE );
    _mask.init(0xffffffff);
    debugView().setDefault(false);

    conf.get( "visible", _visible );
    conf.get( "opacity", _opacity);
    conf.get( "min_range", _minVisibleRange );
    conf.get( "max_range", _maxVisibleRange );
    conf.get( "attenuation_range", _attenuationRange );
    conf.get( "mask", _mask );
    conf.get( "blend", "interpolate", _blend, BLEND_INTERPOLATE );
    conf.get( "blend", "modulate", _blend, BLEND_MODULATE );
}
#endif

//........................................................................

VisibleLayer::VisibleLayer() :
    super()
{
    construct(Config());
}

VisibleLayer::VisibleLayer(const Config& conf) :
    super(conf)
{
    construct(conf);
}

void
VisibleLayer::construct(const Config& conf)
{
    _visible.setDefault(true);
    _opacity.setDefault(1.0f);
    _minVisibleRange.setDefault(0.0);
    _maxVisibleRange.setDefault(FLT_MAX);
    _attenuationRange.setDefault(0.0f);
    _blend.setDefault(BLEND_INTERPOLATE);
    _mask.setDefault(0xffffffff);
    _debugView.setDefault(false);

    conf.get("visible", _visible);
    conf.get("opacity", _opacity);
    conf.get("min_range", _minVisibleRange);
    conf.get("max_range", _maxVisibleRange);
    conf.get("attenuation_range", _attenuationRange);
    conf.get("mask", _mask);
    conf.get("blend", "interpolate", _blend, BLEND_INTERPOLATE);
    conf.get("blend", "modulate", _blend, BLEND_MODULATE);
}

Config
VisibleLayer::getConfig() const
{
    auto conf = Layer::getConfig();
    conf.set("visible", _visible);
    conf.set("opacity", _opacity);
    conf.set("min_range", _minVisibleRange);
    conf.set("max_range", _maxVisibleRange);
    conf.set("attenuation_range", _attenuationRange);
    conf.set("mask", _mask);
    conf.set("blend", "interpolate", _blend, BLEND_INTERPOLATE);
    conf.set("blend", "modulate", _blend, BLEND_MODULATE);
    return conf;
}

Status
VisibleLayer::openImplementation(const IOOptions& io)
{
    Status parent = Layer::openImplementation(io);
    if (parent.failed())
        return parent;

    if (_visible.has_value() || _mask.has_value())
    {
        setVisible(_visible.get());
    }

    return Status::NoError;
}

#if 0
void
VisibleLayer::prepareForRendering(TerrainEngine* engine)
{
    Layer::prepareForRendering(engine);

    initializeUniforms();

    if (options().minVisibleRange().has_value() || options().maxVisibleRange().has_value())
    {
        initializeMinMaxRangeShader();
    }
}
#endif

void
VisibleLayer::setVisible(bool value)
{
    _visible = value;

#if 0
    // if this layer has a scene graph node, toggle its node mask
    osg::Node* node = getNode();
    if (node)
    {
        if (value == true)
        {
            if (_noDrawCallback.valid())
            {
                node->removeCullCallback(_noDrawCallback.get());
                _noDrawCallback = nullptr;
            }
        }
        else
        {
            if (!_noDrawCallback.valid())
            {
                _noDrawCallback = new NoDrawCullCallback();
                node->addCullCallback(_noDrawCallback.get());
            }
        }
    }
#endif

    //fireCallback(&VisibleLayerCallback::onVisibleChanged);
}

void
VisibleLayer::setColorBlending(ColorBlending value)
{
    _blend = value;
 
    ROCKY_TODO();
#if 0
    if (_opacityU.valid())
    {
        _opacityU = nullptr;
        initializeUniforms();
    }
#endif
}

ColorBlending
VisibleLayer::getColorBlending() const
{
    return _blend.value();
}

#if 0
unsigned
VisibleLayer::getMask() const
{
    return options().mask().get();
}

void
VisibleLayer::setMask(osg::Node::NodeMask mask)
{
    // Set the new mask value
    options().mask() = mask;

    // Call setVisible to make sure the mask gets applied to a node if necessary
    setVisible(options().visible().get());
}

osg::Node::NodeMask
VisibleLayer::getDefaultMask()
{
    return DEFAULT_LAYER_MASK;
}

void
VisibleLayer::setDefaultMask(osg::Node::NodeMask mask)
{
    DEFAULT_LAYER_MASK = mask;
}
#endif

bool
VisibleLayer::getVisible() const
{
    return _visible.value();
}

#if 0
void
VisibleLayer::initializeUniforms()
{
    if (!_opacityU.valid())
    {
        osg::StateSet* stateSet = getOrCreateStateSet();

        _opacityU = new osg::Uniform("oe_VisibleLayer_opacityUniform", (float)options().opacity().get());
        stateSet->addUniform(_opacityU.get());

        VirtualProgram* vp = VirtualProgram::getOrCreate(stateSet);
        vp->setName(className());

        vp->setFunction("oe_VisibleLayer_initOpacity", opacityVS, VirtualProgram::LOCATION_VERTEX_MODEL);

        if (options().blend() == BLEND_MODULATE)
        {
            vp->setFunction("oe_VisibleLayer_setOpacity", 
                opacityModulateFS, 
                VirtualProgram::LOCATION_FRAGMENT_COLORING,
                1.1f);

            stateSet->setAttributeAndModes(
                new osg::BlendFunc(GL_DST_COLOR, GL_ZERO),
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        }
        else
        {
            // In this case the fragment shader of the layer is responsible for
            // incorporating the final value of oe_layer_opacity.

            vp->setFunction("oe_VisibleLayer_setOpacity", 
                opacityInterpolateFS, 
                VirtualProgram::LOCATION_FRAGMENT_COLORING,
                1.1f);

            stateSet->setAttributeAndModes(
                new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        }
    }    

    if (!_rangeU.valid())
    {
        osg::StateSet* stateSet = getOrCreateStateSet();

        _rangeU = new osg::Uniform("oe_VisibleLayer_ranges", osg::Vec3f(
            (float)options().minVisibleRange().get(),
            (float)options().maxVisibleRange().get(),
            (float)options().attenuationRange().get()));

        stateSet->addUniform(_rangeU.get());
    }
}

void
VisibleLayer::initializeMinMaxRangeShader()
{
    initializeUniforms();

    if (!_minMaxRangeShaderAdded)
    {
        VirtualProgram* vp = VirtualProgram::getOrCreate(getOrCreateStateSet());
        vp->setName(className());
        vp->setFunction("oe_VisibleLayer_applyMinMaxRange", rangeOpacityVS, VirtualProgram::LOCATION_VERTEX_VIEW);
        _minMaxRangeShaderAdded = true;
    }
}
#endif

void
VisibleLayer::setOpacity(float value)
{
    _opacity = value;
    //initializeUniforms();
    //_opacityU->set(value);
    //fireCallback(&VisibleLayerCallback::onOpacityChanged);
}

float
VisibleLayer::getOpacity() const
{
    return _opacity.value();
}

void
VisibleLayer::setMinVisibleRange(float value)
{
    //initializeMinMaxRangeShader();

    _minVisibleRange = value;
    //options().minVisibleRange() = minVisibleRange;
    //_rangeU->set(osg::Vec3f(
    //    (float)options().minVisibleRange().get(),
    //    (float)options().maxVisibleRange().get(),
    //    (float)options().attenuationRange().get()));
    //fireCallback( &VisibleLayerCallback::onVisibleRangeChanged );
}

float
VisibleLayer::getMinVisibleRange() const
{
    return _minVisibleRange.value(); 
}

void
VisibleLayer::setMaxVisibleRange(float value)
{
    //initializeMinMaxRangeShader();

    _maxVisibleRange = value;
    //options().maxVisibleRange() = maxVisibleRange;
    //_rangeU->set(osg::Vec3f(
    //    (float)options().minVisibleRange().get(),
    //    (float)options().maxVisibleRange().get(),
    //    (float)options().attenuationRange().get()));
    //fireCallback( &VisibleLayerCallback::onVisibleRangeChanged );
}

float
VisibleLayer::getMaxVisibleRange() const
{
    return _maxVisibleRange.value();
    //return options().maxVisibleRange().get();
}

void
VisibleLayer::setAttenuationRange(float value)
{
    //initializeMinMaxRangeShader();

    _attenuationRange = value;
    //options().attenuationRange() = value;
    //_rangeU->set(osg::Vec3f(
    //    (float)options().minVisibleRange().get(),
    //    (float)options().maxVisibleRange().get(),
    //    (float)options().attenuationRange().get()));
}

float
VisibleLayer::getAttenuationRange() const
{
    return _attenuationRange.value();
    //return options().attenuationRange().get();
}
//
//void
//VisibleLayer::fireCallback(VisibleLayerCallback::MethodPtr method)
//{
//    //for (CallbackVector::iterator i = _callbacks.begin(); i != _callbacks.end(); ++i)
//    //{
//    //    VisibleLayerCallback* cb = dynamic_cast<VisibleLayerCallback*>(i->get());
//    //    if (cb) (cb->*method)(this);
//    //}
//}

void
VisibleLayer::setEnableDebugView(bool value)
{
    _debugView = value;
    //if (options().debugView().get() != value)
    //{
    //    //if (value)
    //    //{
    //    //    VirtualProgram* vp = VirtualProgram::getOrCreate(getOrCreateStateSet());
    //    //    ShaderLoader::load(vp, debugViewFS);
    //    //}
    //    //else
    //    //{
    //    //    VirtualProgram* vp = VirtualProgram::get(getStateSet());
    //    //    ShaderLoader::unload(vp, debugViewFS);
    //    //}
    //    options().debugView() = value;
    //}
}

bool
VisibleLayer::getEnableDebugView() const
{
    return _debugView.value();
    //return options().debugView() == true;
}