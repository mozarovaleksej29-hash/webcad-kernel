#include <emscripten/bind.h>
#include <vector>
#include <cmath>
#include <cstdint>

using namespace emscripten;

// ============================================================
// СТРУКТУРЫ ДЛЯ ПЕРЕДАЧИ ДАННЫХ В JS
// ============================================================
struct MeshData {
    std::vector<float> vertices;    // x, y, z для каждой вершины
    std::vector<uint32_t> triangles; // индексы вершин
    float volume;
};

// ============================================================
// ВЫЧИТАНИЕ (УПРОЩЁННОЕ, НО РАБОЧЕЕ)
// ============================================================
MeshData subtract(
    std::vector<float> baseVerts, std::vector<uint32_t> baseTris,
    std::vector<float> toolVerts, std::vector<uint32_t> toolTris
) {
    MeshData result;
    
    // В реальном CSG здесь должен быть BSP-алгоритм.
    // Пока делаем "заглушку" — просто возвращаем базу.
    // Но структура уже готова к передаче в JS.
    
    result.vertices = baseVerts;
    result.triangles = baseTris;
    result.volume = 0.0f; // TODO: посчитать объём
    
    return result;
}

// ============================================================
// ЭКСПОРТ В JS
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
