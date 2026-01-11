// Robust OBJ loader: supports faces in formats:
//  f v v v...
//  f v/vt v/vt v/vt...
//  f v//vn v//vn v//vn...
//  f v/vt/vn ...
// and polygons (triangulates n-gons). Handles negative indices.
//
// Rewritten to use std::getline + token parsing instead of fscanf_s so
// it accepts varied face formats.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>

#include "OBJloader.hpp"

bool loadOBJ(const char * path, std::vector< glm::vec3 > & out_vertices, std::vector< glm::vec2 > & out_uvs, std::vector< glm::vec3 > & out_normals)
{
    out_vertices.clear();
    out_uvs.clear();
    out_normals.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Impossible to open the file: " << path << std::endl;
        return false;
    }

    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_uvs;
    std::vector<glm::vec3> temp_normals;

    // Indices in OBJ are 1-based. We'll collect final indexed lists by unrolling.
    std::vector<unsigned int> vertexIndices;
    std::vector<unsigned int> uvIndices;
    std::vector<unsigned int> normalIndices;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "v") {
            glm::vec3 v;
            ss >> v.x >> v.y >> v.z;
            temp_vertices.push_back(v);
        }
        else if (prefix == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            // flip V for OpenGL like before
            uv.y = 1.0f - uv.y;
            temp_uvs.push_back(uv);
        }
        else if (prefix == "vn") {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            temp_normals.push_back(n);
        }
        else if (prefix == "f") {
            // read all face tokens (supports triangles, quads, ngons)
            std::vector<std::string> tokens;
            std::string tok;
            while (ss >> tok) tokens.push_back(tok);
            if (tokens.size() < 3) {
                // invalid face
                std::cerr << "Face with fewer than 3 vertices: \"" << line << "\"" << std::endl;
                continue;
            }

            // parse tokens into per-face index arrays
            std::vector<int> f_v, f_vt, f_vn;
            f_v.reserve(tokens.size());
            f_vt.reserve(tokens.size());
            f_vn.reserve(tokens.size());

            for (auto &t : tokens) {
                int vi = 0, vti = 0, vni = 0;
                // token can be: v, v/vt, v//vn, v/vt/vn
                size_t p1 = t.find('/');
                if (p1 == std::string::npos) {
                    // only vertex index
                    vi = std::stoi(t);
                }
                else {
                    std::string a = t.substr(0, p1);
                    size_t p2 = t.find('/', p1 + 1);
                    if (p2 == std::string::npos) {
                        // form v/vt
                        if (!a.empty()) vi = std::stoi(a);
                        std::string b = t.substr(p1 + 1);
                        if (!b.empty()) vti = std::stoi(b);
                    }
                    else {
                        // form v//vn or v/vt/vn
                        std::string b = t.substr(p1 + 1, p2 - p1 - 1);
                        std::string c = t.substr(p2 + 1);
                        if (!a.empty()) vi = std::stoi(a);
                        if (!b.empty()) vti = std::stoi(b); // empty means missing vt (v//vn)
                        if (!c.empty()) vni = std::stoi(c);
                    }
                }

                // convert negative indices to positive (relative to current temp arrays)
                auto fix = [](int idx, size_t size)->int {
                    if (idx < 0) return static_cast<int>(size) + idx + 1;
                    return idx;
                };
                if (vi != 0) f_v.push_back(fix(vi, temp_vertices.size()));
                else f_v.push_back(0);
                if (vti != 0) f_vt.push_back(fix(vti, temp_uvs.size()));
                else f_vt.push_back(0);
                if (vni != 0) f_vn.push_back(fix(vni, temp_normals.size()));
                else f_vn.push_back(0);
            }

            // triangulate polygon using fan (0, i, i+1)
            for (size_t i = 1; i + 1 < f_v.size(); ++i) {
                // vertex indices
                vertexIndices.push_back(static_cast<unsigned int>(f_v[0]));
                vertexIndices.push_back(static_cast<unsigned int>(f_v[i]));
                vertexIndices.push_back(static_cast<unsigned int>(f_v[i+1]));

                // uv indices (may be 0)
                uvIndices.push_back(static_cast<unsigned int>(f_vt[0]));
                uvIndices.push_back(static_cast<unsigned int>(f_vt[i]));
                uvIndices.push_back(static_cast<unsigned int>(f_vt[i+1]));

                // normal indices (may be 0)
                normalIndices.push_back(static_cast<unsigned int>(f_vn[0]));
                normalIndices.push_back(static_cast<unsigned int>(f_vn[i]));
                normalIndices.push_back(static_cast<unsigned int>(f_vn[i+1]));
            }
        }
        // ignore other prefixes
    }

    // Unroll indices into direct arrays (matching previous behavior)
    for (size_t i = 0; i < vertexIndices.size(); ++i) {
        unsigned int vi = vertexIndices[i];
        if (vi == 0 || vi > temp_vertices.size()) {
            std::cerr << "Vertex index out of range in OBJ: " << vi << std::endl;
            return false;
        }
        glm::vec3 vertex = temp_vertices[vi - 1];
        out_vertices.push_back(vertex);

        unsigned int uvi = (i < uvIndices.size()) ? uvIndices[i] : 0;
        if (uvi != 0) {
            if (uvi > temp_uvs.size()) {
                // missing uv index referenced
                out_uvs.push_back(glm::vec2(0.0f, 0.0f));
            } else {
                out_uvs.push_back(temp_uvs[uvi - 1]);
            }
        } else {
            out_uvs.push_back(glm::vec2(0.0f, 0.0f));
        }

        unsigned int ni = (i < normalIndices.size()) ? normalIndices[i] : 0;
        if (ni != 0) {
            if (ni > temp_normals.size()) {
                out_normals.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
            } else {
                out_normals.push_back(temp_normals[ni - 1]);
            }
        } else {
            out_normals.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
        }
    }

    return true;
}
