#include "ACAnnouncer.h"
#include <iostream>

static accesskit_windows_adapter* g_acAdapter = nullptr;

bool ACAnnouncer::Initialize() {
	if (g_acAdapter) return true;

	g_acAdapter = accesskit_windows_adapter_new(
		GetForegroundWindow(),
		true,
		[](struct accesskit_action_request* request, void* userdata) {
			accesskit_action_request_free(request);
		},
		nullptr
	);

	return g_acAdapter != nullptr;
}

bool ACAnnouncer::Uninitialize() {
	if (g_acAdapter) {
		accesskit_windows_adapter_free(g_acAdapter);
		g_acAdapter = nullptr;
	}
	return true;
}

bool ACAnnouncer::Speak(const char* text, bool interrupt) {
	if (!g_acAdapter || !text) return false;
	accesskit_node* window_node = accesskit_node_new(ACCESSKIT_ROLE_WINDOW);
	accesskit_node* announcement_node = accesskit_node_new(ACCESSKIT_ROLE_STATUS);

	accesskit_node_set_label(announcement_node, text);
	accesskit_node_set_live(announcement_node, interrupt ? ACCESSKIT_LIVE_ASSERTIVE : ACCESSKIT_LIVE_POLITE);

	accesskit_node_push_child(window_node, ANNOUNCEMENT_ID);

	accesskit_tree_update* update = accesskit_tree_update_with_capacity_and_focus(2, ANNOUNCEMENT_ID);

	accesskit_tree* tree = accesskit_tree_new(WINDOW_ID);
	accesskit_tree_update_set_tree(update, tree);

	accesskit_tree_update_push_node(update, WINDOW_ID, window_node);
	accesskit_tree_update_push_node(update, ANNOUNCEMENT_ID, announcement_node);

	accesskit_windows_adapter_update_if_active(g_acAdapter, update);

	return true;
}

bool ACAnnouncer::StopSpeech() {
	return Speak(true);
}

bool ACAnnouncer::GetActive() {
	return g_acAdapter != nullptr && accesskit_windows_adapter_is_active(g_acAdapter);
}