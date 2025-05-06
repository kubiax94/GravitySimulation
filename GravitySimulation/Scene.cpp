#include "Scene.h"

scene_node* scene::create_scene_node(const std::string& n_name) {
	auto* node = new scene_node(n_name, nullptr, this);
	root_->add_child(node);
	return node;
}

scene_node* scene::find_scene_node(const std::string& n_name) const {
	const auto f_nodes = root_->find_node<scene_node>(n_name, search_options::recursive_down);
	return f_nodes.empty() ? nullptr : f_nodes.front();
}

void scene::register_in(component* comp) {

	const type_id_t t_id = comp->get_type_id();
	if (t_id == rigid_body::type_id())
		physics_.add(static_cast<rigid_body*>(comp)); //NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

}

void scene::register_out(component* comp) {
	
}

void scene::add_to_scene(scene_node* n_node) const {
	root_->add_child(n_node);
}

scene::~scene() {

}

scene::scene() : root_(new scene_node("root")), main_camera_(nullptr), time_(new sim::time()),
                 unit_sys_(new unit_system(1e24f, 1e6f, 3.872e6f / 3600.f)), physics_(unit_sys_) {
}

scene::scene(sim::time* time) : root_(new scene_node("root")), main_camera_(nullptr), time_(time),
                                unit_sys_(new unit_system(1e24f, 1e6f, 3.872e6f / 3600.f)), physics_(unit_sys_) {
}


void scene::init() {

	
	if (!main_camera_)
	{
		if (auto* scene_cam = root_->find_component<Camera>())
			main_camera_ = scene_cam;
		else
			{
			auto* cam_node = create_scene_node("Main Camera");
			main_camera_ = cam_node->add_component<Camera>(cam_node);
			
		}

	}
}

void scene::update() {
	float dt = unit_sys_->time(time_->fixed_delta_time) * 20;
	physics_.update(dt);
	//root_->update();
}

void scene::draw() {
	root_->draw();
}
