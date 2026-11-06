#include <emscripten/bind.h>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <memory>

using namespace emscripten;

// ============================================================
// БАЗОВЫЕ СТРУКТУРЫ
// ============================================================
struct Vertex {
    float x, y, z;
    Vertex() : x(0), y(0), z(0) {}
    Vertex(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    
    Vertex operator+(const Vertex& o) const { return Vertex(x+o.x, y+o.y, z+o.z); }
    Vertex operator-(const Vertex& o) const { return Vertex(x-o.x, y-o.y, z-o.z); }
    Vertex operator*(float s) const { return Vertex(x*s, y*s, z*s); }
    float dot(const Vertex& o) const { return x*o.x + y*o.y + z*o.z; }
    Vertex cross(const Vertex& o) const {
        return Vertex(y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x);
    }
    float length() const { return sqrtf(x*x + y*y + z*z); }
    Vertex normalize() const {
        float len = length();
        if (len < 1e-9) return Vertex(0,0,0);
        return Vertex(x/len, y/len, z/len);
    }
    Vertex lerp(const Vertex& o, float t) const {
        return Vertex(x + (o.x-x)*t, y + (o.y-y)*t, z + (o.z-z)*t);
    }
};

struct Plane {
    Vertex normal;
    float w;
    Plane() : normal(0,0,1), w(0) {}
    Plane(const Vertex& n, float _w) : normal(n), w(_w) {}
    
    static Plane fromPoints(const Vertex& a, const Vertex& b, const Vertex& c) {
        Vertex n = (b - a).cross(c - a).normalize();
        return Plane(n, n.dot(a));
    }
};

struct Polygon {
    std::vector<Vertex> vertices;
    Plane plane;
    
    Polygon() {}
    Polygon(const std::vector<Vertex>& v) : vertices(v) {
        if (v.size() >= 3) {
            plane = Plane::fromPoints(v[0], v[1], v[2]);
        }
    }
    
    void flip() {
        std::reverse(vertices.begin(), vertices.end());
        plane.normal = plane.normal * -1.0f;
        plane.w = -plane.w;
    }
};

// ============================================================
// BSP-УЗЕЛ
// ============================================================
class BSPNode {
public:
    Plane plane;
    std::unique_ptr<BSPNode> front;
    std::unique_ptr<BSPNode> back;
    std::vector<Polygon> polygons;
    
    BSPNode() {}
    
    void build(const std::vector<Polygon>& polys) {
        if (polys.empty()) return;
        
        if (polygons.empty() && !front && !back) {
            plane = polys[0].plane;
        }
        
        std::vector<Polygon> frontPolys, backPolys;
        
        for (const auto& p : polys) {
            splitPolygon(p, polygons, polygons, frontPolys, backPolys);
        }
        
        if (!frontPolys.empty()) {
            if (!front) front = std::make_unique<BSPNode>();
            front->build(frontPolys);
        }
        if (!backPolys.empty()) {
            if (!back) back = std::make_unique<BSPNode>();
            back->build(backPolys);
        }
    }
    
    void splitPolygon(const Polygon& poly, std::vector<Polygon>& coplanarFront,
                      std::vector<Polygon>& coplanarBack,
                      std::vector<Polygon>& frontPolys, std::vector<Polygon>& backPolys) {
        const int COPLANAR = 0, FRONT = 1, BACK = 2, SPANNING = 3;
        const float EPSILON = 1e-5f;
        
        int polygonType = 0;
        std::vector<int> types;
        
        for (const auto& v : poly.vertices) {
            float t = plane.normal.dot(v) - plane.w;
            int type = (t < -EPSILON) ? BACK : (t > EPSILON) ? FRONT : COPLANAR;
            polygonType |= type;
            types.push_back(type);
        }
        
        switch (polygonType) {
            case COPLANAR:
                (plane.normal.dot(poly.vertices[0]) - plane.w > 0 ? coplanarFront : coplanarBack).push_back(poly);
                break;
            case FRONT:
                frontPolys.push_back(poly);
                break;
            case BACK:
                backPolys.push_back(poly);
                break;
            case SPANNING: {
                std::vector<Vertex> f, b;
                for (size_t i = 0; i < poly.vertices.size(); i++) {
                    size_t j = (i + 1) % poly.vertices.size();
                    int ti = types[i], tj = types[j];
                    const Vertex& vi = poly.vertices[i];
                    const Vertex& vj = poly.vertices[j];
                    
                    if (ti != BACK) f.push_back(vi);
                    if (ti != FRONT) b.push_back(vi);
                    
                    if ((ti | tj) == SPANNING) {
                        float t = (plane.w - plane.normal.dot(vi)) / plane.normal.dot(vj - vi);
                        Vertex v = vi.lerp(vj, t);
                        f.push_back(v);
                        b.push_back(v);
                    }
                }
                if (f.size() >= 3) frontPolys.push_back(Polygon(f));
                if (b.size() >= 3) backPolys.push_back(Polygon(b));
                break;
            }
        }
    }
    
    void invert() {
        for (auto& p : polygons) p.flip();
        plane.normal = plane.normal * -1.0f;
        plane.w = -plane.w;
        if (front) front->invert();
        if (back) back->invert();
        std::swap(front, back);
    }
    
    // ✅ ИСПРАВЛЕНО: добавлен const
    std::vector<Polygon> clipPolygons(const std::vector<Polygon>& polys) const {
        if (polygons.empty() && !front && !back) return polys;
        
        std::vector<Polygon> frontPolys, backPolys;
        for (const auto& p : polys) {
            // Используем const_cast, чтобы обойти ограничение const
            const_cast<BSPNode*>(this)->splitPolygon(p, frontPolys, backPolys, frontPolys, backPolys);
        }
        
        std::vector<Polygon> result;
        if (front) {
            auto clipped = front->clipPolygons(frontPolys);
            result.insert(result.end(), clipped.begin(), clipped.end());
        } else {
            result.insert(result.end(), frontPolys.begin(), frontPolys.end());
        }
        
        if (back) {
            auto clipped = back->clipPolygons(backPolys);
            result.insert(result.end(), clipped.begin(), clipped.end());
        }
        
        return result;
    }
    
    void clipTo(const BSPNode& other) {
        polygons = other.clipPolygons(polygons);
        if (front) front->clipTo(other);
        if (back) back->clipTo(other);
    }
    
    void allPolygons(std::vector<Polygon>& out) const {
        out.insert(out.end(), polygons.begin(), polygons.end());
        if (front) front->allPolygons(out);
        if (back) back->allPolygons(out);
    }
};

// ============================================================
// СТРУКТУРА ДЛЯ ПЕРЕДАЧИ В JS
// ============================================================
struct MeshData {
    std::vector<float> vertices;
    std::vector<uint32_t> triangles;
    float volume;
};

// ============================================================
// ВЫЧИТАНИЕ ЧЕРЕЗ BSP
// ============================================================
MeshData subtract(
    std::vector<float> baseVerts, std::vector<uint32_t> baseTris,
    std::vector<float> toolVerts, std::vector<uint32_t> toolTris
) {
    // 1. Конвертируем входные данные в полигоны
    std::vector<Polygon> basePolygons, toolPolygons;
    
    for (size_t i = 0; i < baseTris.size(); i += 3) {
        std::vector<Vertex> v;
        for (int j = 0; j < 3; j++) {
            uint32_t idx = baseTris[i+j];
            v.push_back(Vertex(baseVerts[idx*3], baseVerts[idx*3+1], baseVerts[idx*3+2]));
        }
        basePolygons.push_back(Polygon(v));
    }
    
    for (size_t i = 0; i < toolTris.size(); i += 3) {
        std::vector<Vertex> v;
        for (int j = 0; j < 3; j++) {
            uint32_t idx = toolTris[i+j];
            v.push_back(Vertex(toolVerts[idx*3], toolVerts[idx*3+1], toolVerts[idx*3+2]));
        }
        toolPolygons.push_back(Polygon(v));
    }
    
    // 2. Строим BSP для базы
    BSPNode baseBSP;
    baseBSP.build(basePolygons);
    
    // 3. Строим BSP для резца и инвертируем его (мы вычитаем)
    BSPNode toolBSP;
    toolBSP.build(toolPolygons);
    toolBSP.invert();
    
    // 4. Вырезаем
    baseBSP.clipTo(toolBSP);
    toolBSP.clipTo(baseBSP);
    toolBSP.invert();
    toolBSP.clipTo(baseBSP);
    toolBSP.invert();
    
    // 5. Собираем результат
    std::vector<Polygon> resultPolys;
    baseBSP.allPolygons(resultPolys);
    toolBSP.allPolygons(resultPolys);
    
    // 6. Триангулируем полигоны обратно в сетку
    MeshData result;
    for (const auto& poly : resultPolys) {
        if (poly.vertices.size() < 3) continue;
        
        uint32_t idx0 = result.vertices.size() / 3;
        result.vertices.push_back(poly.vertices[0].x);
        result.vertices.push_back(poly.vertices[0].y);
        result.vertices.push_back(poly.vertices[0].z);
        
        for (size_t i = 1; i < poly.vertices.size() - 1; i++) {
            uint32_t idx1 = result.vertices.size() / 3;
            result.vertices.push_back(poly.vertices[i].x);
            result.vertices.push_back(poly.vertices[i].y);
            result.vertices.push_back(poly.vertices[i].z);
            
            uint32_t idx2 = result.vertices.size() / 3;
            result.vertices.push_back(poly.vertices[i+1].x);
            result.vertices.push_back(poly.vertices[i+1].y);
            result.vertices.push_back(poly.vertices[i+1].z);
            
            result.triangles.push_back(idx0);
            result.triangles.push_back(idx1);
            result.triangles.push_back(idx2);
        }
    }
    
    result.volume = 0.0f; // TODO: посчитать
    
    return result;
}

// ============================================================
// ЭКСПОРТ
// ============================================================
EMSCRIPTEN_BINDINGS(my_kernel) {
    value_object<MeshData>("MeshData")
        .field("vertices", &MeshData::vertices)
        .field("triangles", &MeshData::triangles)
        .field("volume", &MeshData::volume);

    register_vector<float>("VectorFloat");
    register_vector<uint32_t>("VectorUint32");

    function("subtract", &subtract);
}
