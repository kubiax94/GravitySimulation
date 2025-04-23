#pragma once
#include "Mesh.h"

static class Shape
{
public:
	static MeshData GenerateSphere(const float radius = 1.f, const int segements = 64, const int rings = 32);
	static MeshData GenerateGrid(const unsigned int size = 64, float tileSize = 10.f);
	static MeshData GenerateGridLines(const int size = 64, float tileSize = 10.f);
	static MeshData generate_cube();
};

