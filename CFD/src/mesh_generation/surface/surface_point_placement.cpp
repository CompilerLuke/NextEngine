#include "mesh/surface_tet_mesh.h"
#include "mesh_generation/surface_point_placement.h"
#include "mesh_generation/cross_field.h"
#include "mesh_generation/point_octotree.h"
#include "mesh/input_mesh_bvh.h"
#include "numerical/spline.h"
#include "mesh/feature_edges.h"

#include "visualization/debug_renderer.h"

#include "geo/predicates.h"

#include <glm/gtx/rotate_vector.hpp>
#include "visualization/color_map.h"
#include "core/profiler.h"

#include <deque>

float distance_to_triangle(vec3 v[3], vec3 pos) {
    vec3 center = (v[0] + v[1] + v[2]) / 3.0f;
    vec3 normal = triangle_normal(v);
    
    vec3 project = pos - dot(pos - center, normal)*normal;
    
    vec3 c0 = (v[0]+v[1])/2;
    vec3 c1 = (v[1]+v[2])/2;
    vec3 c2 = (v[2]+v[0])/2;
    
    vec3 e0 = v[1]-v[0];
    vec3 e1 = v[2]-v[1];
    vec3 e2 = v[0]-v[2];
    
    vec3 b0 = -normalize(cross(normal, e0));
    vec3 b1 = -normalize(cross(normal, e1));
    vec3 b2 = -normalize(cross(normal, e2));
    
    float disp_e0 = dot(project - c0, b0);
    float disp_e1 = dot(project - c1, b1);
    float disp_e2 = dot(project - c2, b2);
    
    float distance = FLT_MAX;
    if (disp_e0 > 0 && disp_e0 < distance) distance = disp_e0;
    else if (disp_e1 > 0 && disp_e1 < distance) distance = disp_e1;
    else if (disp_e2 > 0 && disp_e2 < distance) distance = disp_e2;
    else distance = 0.0f; //inside
    
    return distance;
}

vertex_handle SurfacePointPlacement::add_vertex(tri_handle tri, vec3 pos) {
    vertex_handle v = { (int)octo.positions.length };
    octo.positions.append(pos);
    octo.add_vert(v);
    tris.append(tri);

    return v;
}

void SurfacePointPlacement::propagate() {
    struct Point {
        tri_handle tri;
        vec3 position;
        float dist;
        float last_dt;
    };

    std::deque<Point> point_queue;

    //suspend_execution(debug);
    //clear_debug_stack(debug);

    float mesh_size = 0.15;
    float dist_mult = 0;  0.5;
    float curv_mult = 0.05;
    
    /*for (uint i = 1; i < mesh.positions.length; i++) {
        vec3 pos = mesh.positions[i];
        float curv = curvatures[i];

        draw_point(debug, pos, color_map(fabs(curv)/1.0));
    }

    suspend_execution(debug);
    */

    Profile discretize("Discretize curves");

    for (FeatureCurve curve : curves) {
        Spline spline = curve.spline;

        float length = spline.total_length;
        float x = 0.0f;

        while (x <= length) {
            float t = spline.const_speed_reparam(x, 1.0);
            vec3 pos = spline.at_time(t);
            
            vec3 tangent = normalize(spline.tangent(t));

            edge_handle e0 = curve.edges[min(t, curve.edges.length-1)];
            edge_handle e1 = mesh.edges[e0];

            float curvature0 = curvatures[mesh.indices[e0].id];
            float curvature1 = curvatures[mesh.indices[e1].id];
            float curvature = (curvature0 + curvature1) / 2.0f;
            float dt = mesh_size / (1 + curv_mult * fabs(curvature));
            
            bool end = x == length;
            x += dt;
            if (!end && x > length) x = length;

            Cross cross1 = cross_field.at_edge(e0);
            Cross cross2 = cross_field.at_edge(e1);
            
            //if (octo.find_closest(pos, cross1, 0.7*dt).id != -1) continue;
            //if (octo.find_closest(pos, cross2, 0.5*mesh_size).id != -1) continue;

            vec3 normal0 = cross1.normal;
            vec3 normal1 = cross2.normal;

            vec3 bitangent0 = normalize(cross(normal0, tangent));
            vec3 bitangent1 = normalize(cross(normal1, tangent));
            
            if (octo.find_closest(pos, cross1, 0.5 * mesh_size).id != -1) continue;
            add_vertex(TRI(e0), pos);

            //draw_point(debug, pos, vec4(1,0,0,1));
            //draw_line(debug, pos, pos + tangent * mesh_size, vec4(1,0,0,1));
            //draw_line(debug, pos, pos - tangent * mesh_size, vec4(1,0,0,1));

            //draw_line(debug, pos, pos - bitangent0 * mesh_size, vec4(0,0,1,1));
            //draw_line(debug, pos, pos + bitangent1 * mesh_size, vec4(0,0,1,1));

            draw_point(debug, pos, RED_DEBUG_COLOR);

            Point point0{ TRI(e0), pos + bitangent0 * mesh_size, mesh_size, mesh_size };
            Point point1{ TRI(e1), pos - bitangent1 * mesh_size, mesh_size, mesh_size };

            point_queue.push_front(point0);
            point_queue.push_front(point1);
        }
    }

    discretize.log();

    Profile place_points("Place points");

    while (!point_queue.empty()) {
        Point point = point_queue.front();
        point_queue.pop_front();

        tri_handle tri = mesh.project(debug, point.tri, &point.position, nullptr);
        if (!tri) continue;

        float curvature = curvatures[mesh.indices[tri].id];
        float last_dt = point.last_dt;
        float dt = mesh_size * (1.0 + dist_mult * point.dist) / (1 + curv_mult *fabs(curvature));

        Cross cross = cross_field.at_tri(tri, point.position);

        vertex_handle found = octo.find_closest(point.position, cross, 0.8 * dt); // / 2.0f);
        if (found.id != -1) {
            //draw_point(debug, point.position, vec4(0,0,1,1));
            continue;
        }

        cross.tangent = normalize(cross.tangent);
        cross.bitangent = normalize(cross.bitangent);

        add_vertex(tri, point.position);
        draw_point(debug, point.position, RED_DEBUG_COLOR);

        vec3 normal = cross.normal;

        Point points[4] = {};
        points[0].position = point.position + cross.tangent * dt;
        points[1].position = point.position - cross.tangent * dt;
        points[2].position = point.position + cross.bitangent * dt;
        points[3].position = point.position - cross.bitangent * dt;

        for (uint i = 0; i < 4; i++) {
            points[i].tri = tri;
            points[i].dist = point.dist + dt;
            points[i].last_dt = dt;
            //draw_point(debug, points[i].position, vec4(0,0,1,1));
            point_queue.push_back(points[i]);
        }

        //suspend_execution(debug);
    }

    place_points.log();

    for (uint i = 0; i < octo.positions.length; i++) {
        draw_point(debug, octo.positions[i], vec4(1, 0, 0, 0));
    }
    
    suspend_execution(debug);
}