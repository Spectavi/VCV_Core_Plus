#pragma once
#include <vector>

#include <common.hpp>
#include <plugin/Plugin.hpp>
#include <plugin/Model.hpp>


namespace rack {


/** Plugin loader and VCV Library synchronizer
*/
namespace plugin {


struct Update {
	std::string pluginSlug;
	std::string pluginName;
	std::string version;
	std::string changelogUrl;
	float progress = 0.f;
};


void init();
void destroy();
void logIn(const std::string& email, const std::string& password);
void logOut();
bool isLoggedIn();
void queryUpdates();
bool hasUpdates();
void syncUpdate(Update* update);
void syncUpdates();
bool isSyncing();
Plugin* getPlugin(const std::string& pluginSlug);
Model* getModel(const std::string& pluginSlug, const std::string& modelSlug);
/** Creates a Model from a JSON module object.
Throws an Exception if the model is not found.
*/
Model* modelFromJson(json_t* moduleJ);
/** Checks that the slug contains only alphanumeric characters, "-", and "_" */
bool isSlugValid(const std::string& slug);
/** Returns a string containing only the valid slug characters. */
std::string normalizeSlug(const std::string& slug);


extern std::vector<Plugin*> plugins;

extern std::string loginStatus;
extern std::vector<Update> updates;
extern std::string updateStatus;
extern bool restartRequested;


} // namespace plugin
} // namespace rack
