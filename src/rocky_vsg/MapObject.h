/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/GeoTransform.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/Switch.h>
#include <vsg/text/Text.h>
#include <list>

namespace ROCKY_NAMESPACE
{
    class Runtime;

    /**
    * Renderable component that attaches to a MapObject, possibly inheriting its
    * GeoTransform and other shared characteristics.
    */
    class ROCKY_VSG_EXPORT Attachment : public rocky::Inherit<Object, Attachment>
    {
    public:
        //! Name of this attachment
        optional<std::string> name;

        //! Whether the attachment should position under the MapObject's GeoTranform
        bool underGeoTransform = false;

        //! Whether the attachment should cull to the visible horizon
        bool horizonCulling = false;

    public:
        //! Whether this attachment should render
        void setVisible(bool value);

        //! Whether this attachment should render
        bool visible() const;

    public:
        //! seialize to JSON
        virtual JSON to_json() const {
            return JSON();
        }

        //! Root node of the attachment; created by createNode()
        vsg::ref_ptr<vsg::Switch> node;

        Attachment() { }

    protected:

        //! Creates the root node of this attachment
        virtual void createNode(Runtime& runtime) { };

        friend class Application;
        friend class AttachmentGroup;
    };

    //! Collection of attachments
    using Attachments = std::vector<shared_ptr<Attachment>>;


    /**
    * Groups multiple attachments together under one attachment.
    */
    class ROCKY_VSG_EXPORT AttachmentGroup : public rocky::Inherit<Attachment, AttachmentGroup>
    {
    public:
        //! Attachments in the group
        Attachments attachments;

        //! Serialize to JSON
        JSON to_json() const override;

    protected:
        void createNode(Runtime& runtime) override;
    };


    /**
    * TODO
    * Interface for the mechanism that will render a particular attachment type.
    * This is a possible avenue for sorting things by state/pipeline?
    * It may be better to simply sort things into groups based on an object "type"
    * that the end application can decide upon.
    */
    class AttachmentRenderer
    {
    };
    using AttachmentRenderers = std::vector<shared_ptr<AttachmentRenderer>>;


    /**
    * An object placed somewhere relative to the map.
    * 
    * A MapObject itself has no visual represetation. You can add attachments
    * to render different things like icons, models, geometries, or text.
    */
    class ROCKY_VSG_EXPORT MapObject : 
        public rocky::Inherit<Object, MapObject>,
        public PositionedObject
    {
    public:
        //! Construct an empty map object.
        MapObject();

        //! Construct a map object with a single attachment.
        MapObject(shared_ptr<Attachment> attachment);

        //! Construct a map object with zero or more attachments.
        MapObject(Attachments attachments);

        //! Globally unique ID for this map object (auto generated)
        const std::uint32_t uid;

        //! Attachments associated with this map object
        Attachments attachments;

        //! Top-level group for this object
        vsg::ref_ptr<vsg::Group> root;

        //! Geotransform for this object if it has any Relative attachments
        vsg::ref_ptr<GeoTransform> xform;

        //! Horizon culler for this object if it requests one
        vsg::ref_ptr<HorizonCullGroup> horizoncull;

    public: // PositionedObject

        const GeoPoint& position() const override;
    };
}
