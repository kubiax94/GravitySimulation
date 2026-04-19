// GravitySimulation.cpp : Ten plik zawiera funkcję „main”. W nim rozpoczyna się i kończy wykonywanie programu.
//
//ONPENGL
//https://pl.wikipedia.org/wiki/Grawitacja

#include <memory>

#include "engine.h"
#include "galactic_scene.h"
#include "galactic_stress_scene.h"
#include "simulation_state.h"
#include "cloth_scene.h"

int main()
{
    engine app;
    if (!app.init(1280, 720, "Test"))
        return -1;

    app.change_state(std::make_unique<simulation_state>(std::make_unique<galactic_stress_scene>(&app.get_time())));
    app.run();
    return 0;
}
