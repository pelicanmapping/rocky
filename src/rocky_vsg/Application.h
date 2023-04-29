/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/InstanceVSG.h>
#include <rocky_vsg/MapNode.h>
#include <rocky_vsg/GeoTransform.h>
#include <rocky_vsg/LineState.h>
#include <rocky_vsg/MeshState.h>

#include <vsg/app/Viewer.h>
#include <vsg/app/Window.h>
#include <vsg/nodes/Group.h>
#include <vsg/text/Text.h>

#include <list>

namespace ROCKY_NAMESPACE
{
    /**
    * Renderable component that attaches to a MapObject, inheriting its
    * GeoTransform and other shared characteristics.
    */
    class ROCKY_VSG_EXPORT Attachment : public rocky::Inherit<Object, Attachment>
    {
    public:
        enum class ReferenceFrame
        {
            Absolute,
            Relative
        };

    public:
        optional<std::string> name;
        vsg::ref_ptr<vsg::Node> node;
        ReferenceFrame referenceFrame = ReferenceFrame::Absolute;
        bool horizonCulling = false;

        //! seialize to JSON
        virtual JSON to_json() const = 0;

    protected:
        Attachment() { }

        virtual void createNode(Runtime& runtime) = 0;

        friend class Application;
    };

    using Attachments = std::vector<shared_ptr<Attachment>>;


    /**
    * Interface for the mechanism that will render a particular attachment type.
    * This is a possible avenue for sorting things by state/pipeline?
    */
    class AttachmentRenderer
    {
    };

    using AttachmentRenderers = std::vector<shared_ptr<AttachmentRenderer>>;


    /**
    * An object placed somewhere on the map.
    * A MapObject itself has no visual represetation. You can add attachments
    * to render different things like icons, models, geometries, or text.
    */
    class ROCKY_VSG_EXPORT MapObject : public rocky::Inherit<Object, MapObject>
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

        //! Horizon culler for this object if it needs one
        vsg::ref_ptr<HorizonCullGroup> horizoncull;
    };


    /**
    * LineString Attachment
    */
    class ROCKY_VSG_EXPORT LineString : public rocky::Inherit<Attachment, LineString>
    {
    public:
        //! Construct a line string attachment
        LineString();

        //! Add a vertex to the end of the line string
        void pushVertex(float x, float y, float z);

        //! Add a vertex to the end of the line string
        template<typename T> void pushVertex(const T& vec3);

        //! Set the rendering style for this line string
        void setStyle(const LineStyle&);

        //! rendering style for the geometry
        const LineStyle& style() const;

        //! serialize as JSON string
        JSON to_json() const override;

    protected:
        void createNode(Runtime& runtime) override;

    private:
        vsg::ref_ptr<engine::BindLineStyle> _bindStyle;
        vsg::ref_ptr<engine::LineStringGeometry> _geometry;
    };

    template<typename T> void LineString::pushVertex(const T& vec3) {
        pushVertex(vec3.x, vec3.y, vec3.z);
    }


    /**
    * Triangle mesh attachment
    */
    class ROCKY_VSG_EXPORT Mesh : public rocky::Inherit<Attachment, Mesh>
    {
    public:
        //! Construct a mesh attachment
        Mesh();

        //! Add a triangle to the mesh
        template<typename VEC3>
        void addTriangle(const VEC3& v1, const VEC3& v2, const VEC3& v3);

        //! Set to overall style for this mesh
        void setStyle(const MeshStyle& value);

        //! Overall style for the mesh
        const MeshStyle& style() const;

        //! serialize as JSON string
        JSON to_json() const override;

    protected:
        void createNode(Runtime& runtime) override;
    
    private:
        vsg::ref_ptr<engine::BindMeshStyle> _bindStyle;
        vsg::ref_ptr<engine::MeshGeometry> _geometry;
    };


    // mesh inline functions

    template<typename VEC3>
    void Mesh::addTriangle(const VEC3& v1, const VEC3& v2, const VEC3& v3)
    {
        _geometry->add(
            vsg::vec3(v1.x, v1.y, v1.z),
            vsg::vec3(v2.x, v2.y, v2.z),
            vsg::vec3(v3.x, v3.y, v3.z));
    }



    class ROCKY_VSG_EXPORT Icon : public rocky::Inherit<Attachment, Icon>
    {
    public:
        Icon() { }
    };


    class ROCKY_VSG_EXPORT Model : public rocky::Inherit<Attachment, Model>
    {
    public:
        Model() { }
    };


    class ROCKY_VSG_EXPORT Label : public rocky::Inherit<Attachment, Label>
    {
    public:
        Label();

        //! Set the text of the label
        void setText(const std::string& value);

        //! Label text
        const std::string& text() const;

        //! serialize as JSON string
        JSON to_json() const override;

    protected:
        void createNode(Runtime&) override;

    private:
        std::string _text;
        vsg::ref_ptr<vsg::Group> _culler;
        vsg::ref_ptr<vsg::Text> _textNode;
    };


    class ROCKY_VSG_EXPORT Application
    {
    public:
        //! Construct a new application object
        Application(int& argc, char** argv);

        //! Create a main window.
        void createMainWindow(int width, int height, const std::string& name = {});

        //! Access the map.
        shared_ptr<Map> map();

        //! Run until exit.
        int run();

        //! Add a map object to the scene
        void add(shared_ptr<MapObject> object);

        //! Remove a map object from the scene
        void remove(shared_ptr<MapObject> object);

    public:

        rocky::InstanceVSG instance;
        vsg::ref_ptr<rocky::MapNode> mapNode;
        vsg::ref_ptr<vsg::Viewer> viewer;
        vsg::ref_ptr<vsg::Window> mainWindow;
        vsg::ref_ptr<vsg::Group> root;
        vsg::ref_ptr<vsg::Group> mainScene;
        std::function<void()> updateFunction;

    public:
        Application(const Application&) = delete;
        Application(Application&&) = delete;

    protected:
        bool _apilayer = false;
        bool _debuglayer = false;
        bool _vsync = true;
        AttachmentRenderers _renderers;

        struct Addition {
            vsg::ref_ptr<vsg::Node> node;
            vsg::CompileResult compileResult;
        };

        std::mutex _add_remove_mutex;
        std::list<util::Future<Addition>> _additions;
        std::list<vsg::ref_ptr<vsg::Node>> _removals;

        void processAdditionsAndRemovals();
    };
}
