#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif // !_USE_MATH_DEFINES

#include "Shape.h"

#include <cmath>
#include <iostream>

MeshData Shape::GenerateSphere(const float radius, const int segements, const int rings) {

	MeshData newData;

	for (int ring = 0; ring <= rings; ring++) {
		float theta = M_PI * ring / rings;
		for (int segment = 0; segment < segements; segment++) {
			Vertex vert;
			float phi = 2 * M_PI * segment / segements;
			vert.Position = { 
				radius * sin(theta) * cos(phi), 
				radius * cos(theta),
				radius * sin(theta) * sin(phi)
			};
			vert.Normal = glm::normalize(vert.Position);

			newData.vertecies.push_back(vert);

		}
	}

	for (int ring = 0; ring <= rings-1; ++ring) {	
		for (int segment = 0; segment < segements; ++segment) {
			int nextSegment = (segment + 1) % segements;

			int v0 = ring * segements + segment;
			int v1 = ring * segements + nextSegment;
			int v2 = (ring + 1) * segements + segment;
			int v3 = (ring + 1) * segements + nextSegment;

			newData.indices.push_back(v0);
			newData.indices.push_back(v1);
			newData.indices.push_back(v2);
			
			newData.indices.push_back(v1);
			newData.indices.push_back(v3);
			newData.indices.push_back(v2);
		}
	}

	return newData;
}

MeshData Shape::GenerateGrid(const unsigned int size, float tileSize) {
	MeshData grid;

	for (int x = 0; x <= size; x++) {
		for (int z = 0; z <= size; z++) {
			Vertex vert;
			vert.Position = {
				x * tileSize,
				0.f,
				z * tileSize
			};
			vert.Normal = glm::vec3(0, 1, 0);
			grid.vertecies.push_back(vert);
		}
	}

	for (int x = 0; x <= size; x++) {
		for (int z = 0; z <= size; z++) {
			
			int i0 = z * (size + 1) + x;
			int i1 = i0 + 1;
			int i2 = i0 + (size + 1);
			int i3 = i2 + 1;

			grid.indices.push_back(i0);
			grid.indices.push_back(i2);
			grid.indices.push_back(i1);

			grid.indices.push_back(i1);
			grid.indices.push_back(i2);
			grid.indices.push_back(i3);

		}
	}

	return grid;
}

MeshData Shape::GenerateGridLines(const int size, float tileSize) {
	MeshData grid;

	for (int x = -size; x <= size; ++x) {
		for (int z = -size; z <= size; ++z) {
			Vertex vert;
			vert.Position = glm::vec3(x * tileSize, 0.f, z * tileSize);
			vert.Normal = glm::vec3(0, 1, 0); // nieistotne tutaj
			grid.vertecies.push_back(vert);
		}
	}

	// Tworzymy linie równoleg³e do osi X i Z

	int lineCount = size * 2 + 1;
	for (int i = 0; i < lineCount; ++i) {
		int offset = i * lineCount;

		// linie pionowe
		grid.indices.push_back(i);
		grid.indices.push_back(i + lineCount * (lineCount - 1));

		// linie poziome
		grid.indices.push_back(i * lineCount);
		grid.indices.push_back(i * lineCount + (lineCount - 1));
	}
	return grid;
}

MeshData Shape::generate_cube() {
	MeshData m_data;

	for (auto x = 0; x < 2; x++)
		for (auto y = 0; y < 2; y++)
			for (auto z = 0; z < 2; z++)
			{
				Vertex v;
				v.Position = glm::vec3(x, y, z) - glm::vec3(0.5f);
				m_data.vertecies.push_back(v);
			}

	return m_data;
}
