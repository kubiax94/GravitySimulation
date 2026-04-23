#include "asset.h"


asset::asset(asset_type type, const std::string& name)
    : resource(type, name) {
}

asset::asset()
    : resource(resource_type::UNKNOWN) {
}
