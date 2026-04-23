#include "Scene.h"
#include "Renderer.h"
#include "aabb_collider.h"
#include "collider.h"
#include "compute_shader.h"
#include "frame_profiler.h"
#include "gpu_fluid_system_component.h"
#include "gpu_particle_system_component.h"

namespace {
void sync_renderer_aabb_collider(renderer& render) {
	auto* node = render.get_node();
	if (!node)
		return;

	auto* aabb = node->find_component<aabb_collider>();
  if (aabb && !aabb->is_auto_generated())
		return;

	if (!aabb) {
		auto* existing_collider = node->find_component<collider>();
		if (existing_collider && !existing_collider->is_auto_generated())
			return;
	}

	const uint64_t renderer_revision = render.get_instance_revision(true);
	const bounding_box local_bounds = render.get_local_bounding_box();
	if (!local_bounds.valid)
		return;

	if (!aabb) {
		aabb = node->add_component<aabb_collider>(node, local_bounds);
		if (!aabb)
			return;
       aabb->set_auto_generated(true);
	}

	if (aabb->get_source_revision() == renderer_revision)
		return;

   aabb->set_auto_generated(true);
	aabb->set_local_bounds(local_bounds);
	aabb->set_source_revision(renderer_revision);
}
}

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
 auto* collider_component = dynamic_cast<collider*>(comp);
	const type_id_t t_id = comp->get_type_id();
  if (t_id == get_type_id<renderer>()) {
		auto* render = static_cast<renderer*>(comp);
		renderers_.push_back(render);
     sync_renderer_aabb_collider(*render);
	}
	else if (t_id == rigid_body::type_id())
		physics_.add(static_cast<rigid_body*>(comp)); //NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
   else if (collider_component)
		physics_.add(collider_component);
   else if (t_id == gpu_fluid_system_component::type_id())
		gpu_fluid_systems_.push_back(static_cast<gpu_fluid_system_component*>(comp));
   else if (t_id == gpu_particle_system_component::type_id())
		gpu_particle_systems_.push_back(static_cast<gpu_particle_system_component*>(comp));
}

void scene::register_out(component* comp) {
	if (!comp)
		return;

 auto* collider_component = dynamic_cast<collider*>(comp);
	const type_id_t t_id = comp->get_type_id();
  if (collider_component) {
		physics_.remove(collider_component);
	}
	else if (t_id == get_type_id<renderer>()) {
		auto* r = static_cast<renderer*>(comp);
		renderers_.erase(std::remove(renderers_.begin(), renderers_.end(), r), renderers_.end());
	}
 else if (t_id == gpu_fluid_system_component::type_id()) {
		auto* system = static_cast<gpu_fluid_system_component*>(comp);
		gpu_fluid_systems_.erase(std::remove(gpu_fluid_systems_.begin(), gpu_fluid_systems_.end(), system), gpu_fluid_systems_.end());
	}
   else if (t_id == gpu_particle_system_component::type_id()) {
		auto* system = static_cast<gpu_particle_system_component*>(comp);
		gpu_particle_systems_.erase(std::remove(gpu_particle_systems_.begin(), gpu_particle_systems_.end(), system), gpu_particle_systems_.end());
	}
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

	if (!scene_loader_.is_started() && scene_loader_.get_total_resources() > 0u)
		scene_loader_.start();
}

void scene::update() {
   scene_loader_.update();

	const float dt = time_->fixed_delta_time;
    std::unordered_set<uuid> gpu_driven_nodes;
	gpu_driven_nodes.reserve(renderers_.size());
	for (const auto* render : renderers_) {
     if (!render || !render->get_node())
			continue;

      sync_renderer_aabb_collider(*const_cast<renderer*>(render));
      if (!render->uses_gpu_driven_positions())
			continue;

		gpu_driven_nodes.insert(render->get_node()->get_id());
	}

	physics_.set_gpu_driven_nodes(gpu_driven_nodes);
    for (auto* system : gpu_fluid_systems_) {
		if (system)
			system->fixed_update(dt);
	}
    for (auto* system : gpu_particle_systems_) {
		if (system)
			system->fixed_update(dt);
	}
	physics_.update(dt);
	//root_->update();
}

void scene::draw() {
	root_->draw();
}

void scene::sync_render() const {
    float alpha = 0.f;
	{
		auto section = frame_profiler::measure_active("scene_sync_render_alpha");
		alpha = time_->interpolation_alpha();
	}

	{
		auto section = frame_profiler::measure_active("scene_sync_render_positions");
		physics_.sync_scene_positions(alpha);
	}

	{
		auto section = frame_profiler::measure_active("scene_sync_render_scene_graph");
		root_->update();
	}
}

size_t scene::get_renderer_physics_index(const renderer* render) const {
	if (!render || !render->get_node())
		return static_cast<size_t>(-1);

	return physics_.get_body_index(render->get_node()->get_id());
}

GLuint scene::get_render_ssbo() const {
	return physics_.get_render_ssbo();
}

void scene::register_compute_shader(compute_shader* c_shader) {
	physics_.register_in(c_shader);
}

void scene::register_compute_shader(compute_shader* c_shader, physics_gpu_stage stage) {
	physics_.register_in(c_shader, stage);
}
