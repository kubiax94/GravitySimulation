#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>
#include <string>

template<typename T, typename  KeyType = std::string>
class base_manager
{
protected:
	std::unordered_map<KeyType, std::unique_ptr<T>> items_;
public:
	virtual ~base_manager() = default;

	void add_item(const KeyType& key, std::unique_ptr<T> item) {
		if (!item || items_.contains(key))
			return;
		items_[key] = std::move(item);
	}

	void remove_item(const KeyType& key) {
		items_.erase(key);
	}

	virtual T* get_item(const KeyType& key) {
		auto it = items_.find(key);
		if (it == items_.end())
			return nullptr;
		return it->second.get();
	}

	std::vector<T*> get_all_items() {
		std::vector<T*> result;
		for (auto& [key, item] : items_) {
			result.push_back(item.get());
		}
		return result;
	}

	virtual std::vector<T*> find_items_by_condition(std::function<bool(const T&)> condition) {
		std::vector<T*> results;
		for (auto& [key, item] : items_) {
			if (condition(*item)) {
				results.push_back(item.get());
			}
		}
		return results;
	}

	virtual size_t size() const {
		return items_.size();
	}
	virtual bool empty() const {
		return items_.empty();
	}
	virtual void clear() {
		items_.clear();
	}
};