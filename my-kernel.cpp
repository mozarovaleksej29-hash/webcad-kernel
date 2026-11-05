#include <emscripten/bind.h>
#include <vector>
#include <cmath>

using namespace emscripten;

// Простая функция для теста: возвращает объём куба
float testVolume(float size) {
    return size * size * size;
}

// Функция вычитания (пока заглушка, потом заменишь на свой CSG)
std::vector<float> subtract(
    std::vector<float> baseVerts, std::vector<int> baseTris,
    std::vector<float> toolVerts, std::vector<int> toolTris
) {
    // TODO: Здесь будет твой алгоритм CSG
    // Пока просто возвращаем базу без изменений
    return baseVerts;
}

EMSCRIPTEN_BINDINGS(my_kernel) {
    function("testVolume", &testVolume);
    function("subtract", &subtract);
}
