/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/Math.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/mat4.h>

namespace rocky
{
    inline fvec3 to_glm(const vsg::vec3& a) {
        return fvec3(a.x, a.y, a.z);
    }
    inline dvec3 to_glm(const vsg::dvec3& a) {
        return dvec3(a.x, a.y, a.z);
    }
    inline fmat4 to_glm(const vsg::mat4& a) {
        return fmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }
    inline dmat4 to_glm(const vsg::dmat4& a) {
        return dmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }

    inline vsg::vec3 to_vsg(const fvec3& a) {
        return vsg::vec3(a.x, a.y, a.z);
    }
    inline vsg::dvec3 to_vsg(const dvec3& a) {
        return vsg::dvec3(a.x, a.y, a.z);
    }
    inline vsg::mat4 to_vsg(const fmat4& a) {
        return vsg::mat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }
    inline vsg::dmat4 to_vsg(const dmat4& a) {
        return vsg::dmat4(
            a[0][0], a[0][1], a[0][2], a[0][3],
            a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3],
            a[3][0], a[3][1], a[3][2], a[3][3]);
    }
}
