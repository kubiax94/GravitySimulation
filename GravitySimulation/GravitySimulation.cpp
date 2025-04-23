// GravitySimulation.cpp : Ten plik zawiera funkcję „main”. W nim rozpoczyna się i kończy wykonywanie programu.
//
//ONPENGL
//https://pl.wikipedia.org/wiki/Grawitacja

#define GLM_ENABLE_EXPERIMENTAL
#include <iostream>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtx/string_cast.hpp>

#include "Shape.h"
#include "Shader.h"
#include "Camera.h"
#include "Time.h"

#include <algorithm>
#include "Renderer.h"
#include "Scene.h"
#include "uuid.h"


void sunShaderUniforms(shader& shader);

const float G = 1.f;

struct Planet {
    std::string name;
    glm::vec3 Position;
    glm::vec3 Velocity;

    float mass;
    glm::vec3 totalForce = glm::vec3(0.0f);
};

std::vector<Planet> planets;
//TENSOR
glm::vec3 forces[7][7];

//SKALOWANIE DO SI ABY ZACHOWAĆ REALIZM.
float scale_mass = 1e24f; // masa Ziemi
float scale_distance = 1e6f; // 1mln km
float scale_time = 3.872e6f / 3600.f; // 1h

float sun_mass_real = 1.9885e30f;
float sun_orbit_real = 0.f;
float sun_velocity_real = 0.f;
float sun_diameter = 1.4e6f / scale_distance;

float earth_mass_real = 5.97e24f;
float earth_orbit_real = 149.6e6f;
float earth_velocity_real = 47.4f;
float eartrh_diameter = 12756.f / scale_distance;

float venus_mass_real = 4.87e24f;
float venus_orbit_real = 108.2e6f;
float venus_velocity_real = 35.4f;

float mercury_mass_real = 0.330e24f;
float mercury_orbit_real = 57.9e6f;
float mercury_velocity_real = 0.f;

float moon_mass_real = 0.073e24f;
float moon_orbit_real = earth_orbit_real +  0.384e6f;
float moon_diameter = 3475.f / scale_distance;

float mars_mass_real = 0.642e24f;
float mars_orbit_real = 228.8e6f;
float mars_diameter = 6792.f / scale_distance;

float planetX_mass_real = 1.642e26f;
float planetX_orbit_real = 145.8e6f;

float visual_render_scale = 40.f;

int main()
{
    //Planet sun({ "Sun", {0.f,0.f,0.f},{0.f, 0.f, 0.f}, sun_mass_real / scale_mass });
    //planets.push_back(sun);

    //float orbit_r = earth_orbit_real / scale_distance;
    //float sun_mass = sun_mass_real / scale_mass;
    ////float v = earth_velocity_real * scale_time / scale_distance;
    //float v = sqrt(G * sun_mass / orbit_r);
    //Planet earth({ "Earth", {orbit_r, 0, 0}, {0, v, 0}, earth_mass_real / scale_mass });
    //std::cout << "Velocity: " << v << "Position: " << orbit_r << "Earth Diameter: " << eartrh_diameter;
    //planets.push_back(earth);

    //orbit_r = venus_orbit_real / scale_distance;
    ////v = venus_velocity_real * scale_time / scale_distance;
    //v = sqrt(G * sun_mass / orbit_r);

    //planets.push_back({ "Venus", {orbit_r, 0, 0}, {0, v, 0}, venus_mass_real / scale_mass});

    //orbit_r = mercury_orbit_real / scale_distance;
    //v = sqrt(G * sun_mass / orbit_r);

    //planets.push_back({"Mercury", {orbit_r, 0, 0}, {0, v, 0}, mercury_mass_real / scale_mass});


    //glm::vec3 earthPos = earth.Position;
    //glm::vec3 moonOffset = glm::vec3(.384f, 0.f, 0.f);

    //glm::vec3 moonPos = earthPos + moonOffset;

    //float moonOrbitRadious = glm::length(moonOffset);
    //float earthMass = earth.mass;

    //float moonV = sqrt(G * earthMass / moonOrbitRadious);

    //glm::vec3 orbitPlaneNormal = glm::vec3(0, 1, 0);
    //glm::vec3 moonDir = glm::normalize(glm::cross(glm::normalize(moonOffset), orbitPlaneNormal));

    //glm::vec3 moonVel = earth.Velocity + moonDir * moonV;

    //planets.push_back({"Moon", moonPos, moonVel, moon_mass_real / scale_mass});

    //orbit_r = mars_orbit_real / scale_distance;
    //v = sqrt(G * sun_mass / orbit_r);
    //planets.push_back({"Mars" ,{orbit_r, 0, 0}, {0, v, 0}, mars_mass_real / scale_mass });

    //orbit_r = planetX_orbit_real / scale_distance;

    //v = sqrt(G * sun_mass / orbit_r);

    ////planets.push_back(Planet({ "PlanetX",{orbit_r,0.f,0.f},{0.0f, v, 0.f}, planetX_mass_real / scale_mass}));
    ////planets.push_back(Planet({ {6.f,5.f,0.f},{0.0f, 0.f, 0.f}, 21.f }));


    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

   
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Test", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);



    glEnable(GL_MULTISAMPLE);

    auto sphereVert = Shape::GenerateSphere();
    auto gridVert = Shape::GenerateGridLines(64, 50.f);
    Mesh sphereMesh(sphereVert);
    Mesh gridMesh(gridVert);
    gridMesh.type = MeshType::LINES;
    shader _shader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/camera.vs.shader", "C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/camera.fs.shader");
    shader gridShader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/default.vs.shader", "C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/default.fs.shader");
    shader sunShader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/lightsource.vs.shader", "C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/sun.fs.shader");
    glUniform3fv(glGetUniformLocation(sunShader.id, "lightColor"), 1, &glm::vec3(1.0f, .8f, .3f)[0]);

    sim::time* time1 = new sim::time();


    scene cscene(time1);
    scene_node* cam_node = cscene.create_scene_node("cam");
    scene_node* sun_node = cscene.create_scene_node("sun");
    scene_node* grid_node = cscene.create_scene_node("grid");

    renderer sunRenderer(sun_node, &_shader, &sphereMesh);

    sun_node->add_component<renderer>(sun_node, &gridShader, &sphereMesh);

    cam_node->add_component<Camera>(cam_node);
    cam_node->set_position(glm::vec3(0.f, 245.f, 300.f));
    cam_node->set_rotation(glm::vec3(-45.f, 0.f, 0.f));
    auto* cam = cam_node->find_component<Camera>();
    //sunRenderer.attach_to(sun_node);

    for (int i = 0; i < 10; i++)
    {
    	scene_node* node = new scene_node("test" + std::to_string(i));
    	scene_node* node1 = new scene_node("test" + std::to_string(i) + std::to_string(i));

        node->add_component<renderer>(node, &_shader, &sphereMesh);
        node->add_child(node1);
        sun_node->add_child(node);

    }
    renderer grid_renderer(grid_node, &gridShader, &gridMesh);
    grid_renderer.attach_to(grid_node);


    float x = 0;

    grid_node->set_position(glm::vec3(.0f, .001f, .0f));
    sun_node->set_position(glm::vec3(0,0,0));
    sun_node->set_scale(glm::vec3(20));

    float rot = 0;

    std::vector<scene_node*> find_test = sun_node->find_node<scene_node>("test11", search_options::all_node);
    find_test[0]->set_position(10, 0, 0);
    auto vel = glm::vec3(1, 0, 0);
    find_test[0]->add_component<renderer>(find_test[0], &gridShader, &sphereMesh);

    auto guid = uuid();

    std::cout << guid << std::endl;
    auto p_data = physics_data(10.f, glm::vec3( 0,0,0 ), { 0,0,0 });
    sun_node->add_component<rigid_body>(sun_node, p_data);
	while (!glfwWindowShouldClose(window)) {

        glEnable(GL_DEPTH_TEST);

        time1->update_time(static_cast<float>(glfwGetTime()));


        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        while (time1->should_fixed_update())
        {
            //for (int i = 0; i < planets.size(); ++i) {
            //    std::cout << "Planet: " << planets[i].name << " vel: " << glm::length(planets[i].Velocity) << std::endl;
            //    for (int j = i+1; j < planets.size(); ++j) {

            //        glm::vec3 direction = planets[j].Position - planets[i].Position;
            //        float distance = glm::length(direction);
            //        direction = glm::normalize(direction);

            //        float forceMagnitude = (G * planets[i].mass * planets[j].mass) / (distance * distance + 1e-5f); // epsilon do uniknięcia dzielenia przez 0
            //        glm::vec3 force = direction * forceMagnitude;

            //        planets[i].totalForce += force;
            //        planets[j].totalForce -= force;

            //        if (i == 0 && j == 1) {
            //            std::cout << "forceMagnitude[0][1]: " << forceMagnitude << std::endl;
            //        }
            //    }
            //}

            //for (int i = 0; i < planets.size(); ++i) {

            //    glm::vec3 acc = planets[i].totalForce / planets[i].mass;
            //    planets[i].Velocity += acc * time1.fixed_delta_time;
            //    planets[i].Position += planets[i].Velocity * time1.fixed_delta_time;
            //    planets[i].totalForce = glm::vec3(0.f);

            //   std::cout << planets[i].name << " -> " << " X: " << planets[i].Position.x << " Y: " << planets[i].Position.y << " Z: " << planets[i].Position.z << std::endl;
            //   std::cout << planets[i].name << " Lenght to SUN -> " << glm::length(planets[i].Position - sun.Position) << std::endl;
            //}

            cscene.update();
            time1->reduce_accumulator();
        }


        
        //for (int i = 0; i < planets.size(); i++) {

        //    if (planets[i].name == "Sun") {
        //        sunRenderer.transform->setPosition(planets[i].Position);
        //        auto scale = glm::vec3(20.f);
        //        sunRenderer.transform->setScale(scale);

        //        sunRenderer.draw(&cam, [&](shader& s) {
        //            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
        //            s.set_uni_vec3("viewPos", cam.transofrm.GetPosition());
        //            s.set_uni_float("intensity", 2.f);
        //            //glUniform3fv(glGetUniformLocation(s.ID, "lightColor"), 1, &glm::vec3(1.0f, .8f, .3f)[0]);
        //            //glUniform1f(glGetUniformLocation(s.ID, "time"), time.current);
        //        });
        //    }
        //    else if (planets[i].name == "Moon") {

        //        renderer.transform->setPosition(planets[i].Position);
        //        auto scale = glm::vec3(10.f);
        //        renderer.transform->setScale(scale);

        //        renderer.transform->Translate(glm::vec3(25.f, 0, 0));


        //        renderer.draw(&cam, [&](shader& s) {

        //            s.set_uni_vec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
        //            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
        //            s.set_uni_vec3("viewPos", cam.transofrm.GetPosition());
        //            s.set_uni_vec3("lightPos", sun.Position);

        //            //glUniform3fv(glGetUniformLocation(s.ID, "objectColor"), 1, &glm::vec3(1.0f, 0.5f, 0.31f)[0]);
        //            //glUniform3fv(glGetUniformLocation(s.ID, "lightColor"), 1, &glm::vec3(1.0f, .8f, .3f)[0]);
        //            //glUniform3fv(glGetUniformLocation(s.ID, "viewPos"), 1, &cam.GetViewMatrix()[0][0]);
        //            //glUniform3fv(glGetUniformLocation(s.ID, "lightPos"), 1, &sun.Position[0]);
        //        });
        //    }
        //    else {
        //        renderer.transform->setPosition(planets[i].Position);
        //        auto scale = glm::vec3(10.f);
        //        renderer.transform->setScale(scale);

        //        renderer.draw(&cam, [&](shader& s) {
        //            glUniform3fv(glGetUniformLocation(s.id, "objectColor"), 1, &glm::vec3(1.0f, 0.5f, 0.31f)[0]);
        //            glUniform3fv(glGetUniformLocation(s.id, "lightColor"), 1, &glm::vec3(1.0f, .8f, .3f)[0]);
        //            glUniform3fv(glGetUniformLocation(s.id, "viewPos"), 1, &cam.transofrm.GetPosition()[0]);
        //            glUniform3fv(glGetUniformLocation(s.id, "lightPos"), 1, &sun.Position[0]);
        //        });
        //    }
        //}

    	sunRenderer.draw(cam, [&](shader& s) {
                        s.set_uni_vec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
                        s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
                        s.set_uni_vec3("viewPos", cam->get_transform()->get_global_position());
                        s.set_uni_vec3("lightPos", sun_node->get_position());
            //glUniform3fv(glGetUniformLocation(s.ID, "lightColor"), 1, &glm::vec3(1.0f, .8f, .3f)[0]);
            //glUniform1f(glGetUniformLocation(s.ID, "time"), time.current);
        });
        find_test[0]->find_component<renderer>(search_options::first | search_options::include_self)[0]->draw(cam, [&](shader& s) {
            s.set_uni_vec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
            s.set_uni_vec3("viewPos", cam->get_transform()->get_global_position());
            s.set_uni_vec3("lightPos", sun_node->get_position());
            //glUniform3fv(glGetUniformLocation(s.ID, "lightColor"), 1, &glm::vec3(1.0f, .8f, .3f)[0]);
            //glUniform1f(glGetUniformLocation(s.ID, "time"), time.current);
        });

        grid_renderer.draw(cam);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();
    return 0;
    
}

// Uruchomienie programu: Ctrl + F5 lub menu Debugowanie > Uruchom bez debugowania
// Debugowanie programu: F5 lub menu Debugowanie > Rozpocznij debugowanie

// Porady dotyczące rozpoczynania pracy:
//   1. Użyj okna Eksploratora rozwiązań, aby dodać pliki i zarządzać nimi
//   2. Użyj okna programu Team Explorer, aby nawiązać połączenie z kontrolą źródła
//   3. Użyj okna Dane wyjściowe, aby sprawdzić dane wyjściowe kompilacji i inne komunikaty
//   4. Użyj okna Lista błędów, aby zobaczyć błędy
//   5. Wybierz pozycję Projekt > Dodaj nowy element, aby utworzyć nowe pliki kodu, lub wybierz pozycję Projekt > Dodaj istniejący element, aby dodać istniejące pliku kodu do projektu
//   6. Aby w przyszłości ponownie otworzyć ten projekt, przejdź do pozycji Plik > Otwórz > Projekt i wybierz plik sln
