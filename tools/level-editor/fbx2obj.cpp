// fbx2obj: bake an FBX to a plain OBJ (positions, normals, UVs) for the
// level editor's asset blob. Uses the repo's vendored ufbx, like the
// engine's creature bakers. Usage: fbx2obj input.fbx output.obj
#include "ufbx.h"
#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: fbx2obj input.fbx output.obj\n"); return 1; }
    ufbx_load_opts options{};
    options.target_axes = ufbx_axes_right_handed_y_up;
    options.target_unit_meters = 1.0f;
    ufbx_error error{};
    ufbx_scene* scene = ufbx_load_file(argv[1], &options, &error);
    if (!scene) { std::fprintf(stderr, "fbx2obj: %s\n", error.description.data); return 2; }
    FILE* out = std::fopen(argv[2], "wb");
    if (!out) { ufbx_free_scene(scene); return 3; }
    std::fprintf(out, "# baked by DepthWorks fbx2obj from %s\n", argv[1]);
    size_t base = 0;
    std::vector<uint32_t> indices;
    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        ufbx_node* node = scene->nodes.data[ni];
        ufbx_mesh* mesh = node->mesh;
        if (!mesh || mesh->faces.count == 0) continue;
        indices.assign(mesh->max_face_triangles * 3, 0);
        const size_t vertexCount = mesh->vertices.count;
        for (size_t i = 0; i < vertexCount; ++i) {
            ufbx_vec3 p = ufbx_transform_position(&node->geometry_to_world, mesh->vertex_position[i]);
            std::fprintf(out, "v %.6f %.6f %.6f\n", p.x, p.y, p.z);
        }
        if (mesh->vertex_normal.exists) {
            for (size_t i = 0; i < vertexCount; ++i) {
                ufbx_vec3 n = ufbx_transform_direction(&node->geometry_to_world, mesh->vertex_normal[i]);
                std::fprintf(out, "vn %.6f %.6f %.6f\n", n.x, n.y, n.z);
            }
        }
        if (mesh->vertex_uv.exists) {
            for (size_t i = 0; i < vertexCount; ++i) {
                ufbx_vec2 uv = mesh->vertex_uv[i];
                std::fprintf(out, "vt %.6f %.6f\n", uv.x, uv.y);
            }
        }
        std::fprintf(out, "o %s\n", node->name.data ? node->name.data : "mesh");
        for (size_t fi = 0; fi < mesh->faces.count; ++fi) {
            ufbx_face face = mesh->faces.data[fi];
            size_t tris = ufbx_triangulate_face(indices.data(), indices.size(), mesh, face);
            for (size_t t = 0; t < tris; ++t) {
                std::fprintf(out, "f");
                for (int k = 0; k < 3; ++k) {
                    size_t idx = indices[t * 3 + k] + base + 1;
                    if (mesh->vertex_normal.exists && mesh->vertex_uv.exists)
                        std::fprintf(out, " %zu/%zu/%zu", idx, idx, idx);
                    else if (mesh->vertex_uv.exists)
                        std::fprintf(out, " %zu/%zu", idx, idx);
                    else if (mesh->vertex_normal.exists)
                        std::fprintf(out, " %zu//%zu", idx, idx);
                    else
                        std::fprintf(out, " %zu", idx);
                }
                std::fprintf(out, "\n");
            }
        }
        base += vertexCount;
    }
    std::fclose(out);
    ufbx_free_scene(scene);
    return base ? 0 : 4;
}
