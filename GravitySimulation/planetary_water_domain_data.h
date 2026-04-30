#pragma once

#include <glad/glad.h>

struct planetary_water_domain_desc
{
    int width = 1024;
    int height = 512;
    float planet_radius = 1.0f;
    float shell_thickness = 0.1f;
};

struct planetary_water_domain_textures
{
    GLuint physics_mask_texture = 0;
    GLuint render_mask_texture = 0;
    GLuint continuity_texture = 0;
    GLuint veto_texture = 0;
    GLuint water_level_texture = 0;
    GLuint region_id_texture = 0;
    GLuint shore_distance_texture = 0;
};
