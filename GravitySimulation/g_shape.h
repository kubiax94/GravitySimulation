#pragma once
#include "Mesh.h"

class g_shape
{
public:
	static MeshData generate_sphere(const float radius = 1.f, const int segements = 64, const int rings = 32);
	static MeshData generate_grid(const unsigned int size = 64, float tileSize = 10.f);
	static MeshData generate_grid_lines(const int size = 64, float tileSize = 10.f);
	static MeshData generate_cube();
};

