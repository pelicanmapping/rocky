/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <rocky_vsg/engine/MeshState.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Render settings for a mesh.
     */
    struct MeshStyle
    {
        // if alpha is zero, use the line's per-vertex color instead
        vsg::vec4 color = { 1, 1, 1, 0 };
        float wireframe = 0.0f;
    };

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
        vsg::ref_ptr<BindMeshStyle> _bindStyle;
        vsg::ref_ptr<MeshGeometry> _geometry;
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
}
