// GravitySimulation.cpp : Ten plik zawiera funkcję „main”. W nim rozpoczyna się i kończy wykonywanie programu.
//
//ONPENGL
//https://pl.wikipedia.org/wiki/Grawitacja

#include <memory>

#include "engine.h"
#include "simulation_state.h"

int main()
{
    engine app;
    if (!app.init(1280, 720, "Test"))
        return -1;

    app.change_state(std::make_unique<simulation_state>());
    app.run();
    return 0;
}
