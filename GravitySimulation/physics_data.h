#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/**
 * OpenGl przyjmuje dane w segmentach 16 byte do Vertex Buffor Array
 * dlatego aby nie robić zbędnych danych w vec4 za w przyjmujemy masę
 * dzięki temu mamy 3x16 byte i nie musimy tworzyc dodatkowej pustej
 * vec4 zmiennej aby dopełnic do 16 byte
 */

struct physics_data
{
	//w is mass
	glm::vec4 position = glm::vec4(0.f,0.f,0.f, 1.f);
	glm::vec4 velocity = glm::vec4(0.f, 0.f, 0.f, 1.f);
	glm::vec4 accumulated_force = glm::vec4(0.f, 0.f, 0.f, 1.f);
};
