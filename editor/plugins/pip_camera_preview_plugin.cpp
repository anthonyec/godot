#include "pip_camera_preview_plugin.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/animation/tween.h"
#include "scene/gui/button.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/viewport.h"

constexpr float MIN_PANEL_SIZE = 250;

PIPCameraPreview::PIPCameraPreview(Control *container) {
	this->container = container;
	set_z_index(1);

	// Placeholder that is shown when dragging the panel around.
	placeholder = memnew(Panel);
	placeholder->set_visible(false);
	placeholder->set_modulate(Color(1, 1, 1, 0.5));
	placeholder->set_z_index(0);
	container->add_child(placeholder);

	sub_viewport = memnew(SubViewport);
	add_child(sub_viewport);

	// Viewport texture that renders the preview.
	viewport_texture_container = memnew(MarginContainer);
	add_child(viewport_texture_container);

	viewport_texture = memnew(TextureRect);
	viewport_texture_container->add_child(viewport_texture);

	// Overlay which contains all the button controls.
	float margin_size = 2 * EDSCALE;

	overlay_margin_container = memnew(MarginContainer);
	overlay_margin_container->set_anchors_preset(PRESET_FULL_RECT);
	overlay_margin_container->add_theme_constant_override("margin_left", margin_size);
	overlay_margin_container->add_theme_constant_override("margin_right", margin_size);
	overlay_margin_container->add_theme_constant_override("margin_top", margin_size);
	overlay_margin_container->add_theme_constant_override("margin_bottom", margin_size);
	add_child(overlay_margin_container);

	overlay_container = memnew(Control);
	overlay_container->set_anchors_preset(PRESET_FULL_RECT);
	overlay_margin_container->add_child(overlay_container);

	// Button Controls.
	Vector2 button_size = Vector2(30, 30) * EDSCALE;

	drag_handle = memnew(Button);
	drag_handle->set_anchors_preset(PRESET_FULL_RECT);
	drag_handle->set_flat(true);
	drag_handle->set_focus_mode(FOCUS_NONE);
	drag_handle->connect("button_down", callable_mp(this, &PIPCameraPreview::_on_drag_handle_button_down));
	drag_handle->connect("button_up", callable_mp(this, &PIPCameraPreview::_on_drag_handle_button_up));
	overlay_container->add_child(drag_handle);

	resize_left_handle = memnew(Button);
	resize_left_handle->set_flat(true);
	resize_left_handle->set_icon(EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GuiResizerTopLeft"), EditorStringName(EditorIcons)));
	resize_left_handle->set_size(button_size);
	resize_left_handle->set_pivot_offset(Vector2(0, 0));
	resize_left_handle->set_position(Vector2(0, 0));
	resize_left_handle->set_anchors_preset(PRESET_TOP_LEFT);
	resize_left_handle->connect("button_down", callable_mp(this, &PIPCameraPreview::_on_resize_handle_button_down));
	resize_left_handle->connect("button_up", callable_mp(this, &PIPCameraPreview::_on_resize_handle_button_up));
	overlay_container->add_child(resize_left_handle);

	resize_right_handle = memnew(Button);
	resize_right_handle->set_flat(true);
	resize_right_handle->set_icon(EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GuiResizerTopRight"), EditorStringName(EditorIcons)));
	resize_right_handle->set_size(button_size);
	resize_right_handle->set_pivot_offset(Vector2(button_size.x, 0));
	resize_right_handle->set_position(Vector2(overlay_container->get_size().x - button_size.x, 0));
	resize_right_handle->set_anchors_preset(PRESET_TOP_RIGHT);
	resize_right_handle->connect("button_down", callable_mp(this, &PIPCameraPreview::_on_resize_handle_button_down));
	resize_right_handle->connect("button_up", callable_mp(this, &PIPCameraPreview::_on_resize_handle_button_up));
	overlay_container->add_child(resize_right_handle);

	pin_button = memnew(Button);
	pin_button->set_flat(true);
	pin_button->set_icon(EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("Pin"), EditorStringName(EditorIcons)));
	pin_button->set_size(button_size);
	pin_button->set_pivot_offset(Vector2(0, button_size.y));
	pin_button->set_position(Vector2(0, overlay_container->get_size().y - button_size.y));
	pin_button->set_anchors_preset(PRESET_BOTTOM_LEFT);
	overlay_container->add_child(pin_button);

	// TODO(anthony): I think an ancestor node below `Node3DEditor` is setting
	// process to false (with `set_process`) since this preview does not update.
	// Remove this when I find out which node is causing this.
	set_process_mode(PROCESS_MODE_ALWAYS);
	set_process(true);
};

PIPCameraPreview::~PIPCameraPreview(){};

void PIPCameraPreview::set_inset(real_t left, real_t bottom) {
	inset.left = left;
	inset.bottom = bottom;
}

void PIPCameraPreview::set_camera_3d(Camera3D *camera) {
	camera_3d = camera;
}

void PIPCameraPreview::_bind_methods() {
	ADD_SIGNAL(MethodInfo("pinned_position_changed"));
}

void PIPCameraPreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			container = get_parent_control();
		} break;

		case NOTIFICATION_PROCESS: {
			if (!is_visible()) {
				break;
			}

			switch (state) {
				case INTERACTION_STATE_NONE: {
					set_size(_get_clamped_size(get_size()));
					set_position(_get_pinned_position(pinned_edge));
				} break;

				case INTERACTION_STATE_RESIZE: {
					Vector2 delta_mouse_position = initial_mouse_position - get_global_mouse_position();
					Vector2 resized_size = get_size();

					if (pinned_edge == PINNED_EDGE_LEFT) {
						resized_size = initial_panel_size - delta_mouse_position;
					}

					if (pinned_edge == PINNED_EDGE_RIGHT) {
						resized_size = initial_panel_size + delta_mouse_position;
					}

					set_size(_get_clamped_size(resized_size));
					set_position(_get_pinned_position(pinned_edge));
				} break;

				case INTERACTION_STATE_DRAG: {
					Vector2 global_mouse_position = get_global_mouse_position();
					Vector2 offset = initial_mouse_position - initial_panel_position;

					Vector2 container_size = container->get_size();
					Vector2 container_position = container->get_global_position();
					float halfway_point = container_position.x + (container_size.x / 2);

					PinnedEdge new_pinned_edge;

					if (global_mouse_position.x < halfway_point) {
						new_pinned_edge = PINNED_EDGE_LEFT;
					} else {
						new_pinned_edge = PINNED_EDGE_RIGHT;
					}

					if (pinned_edge != new_pinned_edge) {
						pinned_edge = new_pinned_edge;
						emit_signal(SNAME("pinned_position_changed"));
					}

					set_global_position(_get_clamped_position(global_mouse_position - offset));

					placeholder->set_position(_get_pinned_position(pinned_edge));
					placeholder->set_size(get_size());
				} break;

				case INTERACTION_STATE_START_ANIMATE_INTO_PLACE: {
					Vector2 final_position = _get_pinned_position(pinned_edge);
					Ref<Tween> tween = SceneTree::get_singleton()->create_tween();

					tween->bind_node(this);
					tween->set_ease(Tween::EASE_OUT);
					tween->set_trans(Tween::TRANS_CUBIC);
					tween->tween_property(this, NodePath("position"), final_position, 0.3);
					tween->connect("finished", callable_mp(this, &PIPCameraPreview::_on_animate_into_place_finished).bind(final_position));

					state = INTERACTION_STATE_ANIMATE_INTO_PLACE;
				} break;

				case INTERACTION_STATE_ANIMATE_INTO_PLACE: {
					// This state is used as a holding state while the tween animates
					// the panel.
				} break;
			}

			// TODO(anthony): Maybe there's a nicer way to do this with mouse over
			// signals. I did this originally because I couldn't get it working nicely
			// in the GDScript plugin.
			Rect2 hover_area_rect = Rect2(get_global_position(), get_size());
			hover_area_rect = hover_area_rect.grow(40);

			// Show the controls when hovering or when interaction state is "none" to
			// prevent flickering of the controls during animation.
			show_controls = hover_area_rect.has_point(get_global_mouse_position()) || state != INTERACTION_STATE_NONE;
			overlay_margin_container->set_visible(show_controls);
			resize_left_handle->set_visible(show_controls && pinned_edge == PINNED_EDGE_RIGHT);
			resize_right_handle->set_visible(show_controls && pinned_edge == PINNED_EDGE_LEFT);
			placeholder->set_visible(state == INTERACTION_STATE_DRAG || state == INTERACTION_STATE_ANIMATE_INTO_PLACE);

			if (camera_3d) {
				//...
			}
		} break;
	}
}

Vector2 PIPCameraPreview::_get_pinned_position(PinnedEdge pinned_position) {
	Vector2 margin = Vector2(10, 10) * EDSCALE;
	Vector2 container_size = container->get_size();
	Vector2 panel_size = get_size();

	switch (pinned_position) {
		case PINNED_EDGE_LEFT: {
			return Vector2(0, container_size.y) - Vector2(0, panel_size.y) - Vector2(-margin.x, margin.y) - Vector2(-inset.left, inset.bottom);
		} break;

		case PINNED_EDGE_RIGHT: {
			return container_size - panel_size - margin - Vector2(0, inset.bottom);
		} break;
	}
}

Vector2 PIPCameraPreview::_get_project_window_size() {
	float width = GLOBAL_GET("display/window/size/viewport_width");
	float height = GLOBAL_GET("display/window/size/viewport_height");

	return Vector2(width, height);
}

float PIPCameraPreview::_get_project_window_ratio() {
	Vector2 project_window_size = _get_project_window_size();

	return project_window_size.y / project_window_size.x;
}

Vector2 PIPCameraPreview::_get_clamped_position(Vector2 desired_position) {
	Vector2 clamped_position = desired_position;

	Vector2 container_position = container->get_global_position();
	Vector2 container_size = container->get_size();
	Vector2 panel_size = get_size();

	// TODO(anthony): Can this size be found dynamically, maybe from a focus
	// `StyleBox` somewhere?
	Vector2 focus_ring_size = Vector2(2, 2) * EDSCALE;

	return clamped_position.clamp(container_position + focus_ring_size, container_position + container_size - panel_size - focus_ring_size);
}

Vector2 PIPCameraPreview::_get_clamped_size(Vector2 desired_size) {
	float viewport_ratio = _get_project_window_ratio();
	Vector2 container_size = container->get_size();

	Vector2 max_bounds = Vector2(
			container_size.x * 0.6,
			container_size.y * 0.8);

	Vector2 clamped_size = desired_size;

	// Apply aspect ratio.
	clamped_size = Vector2(clamped_size.x, clamped_size.x * viewport_ratio);

	// Clamp the max size while respecting the aspect ratio.
	if (clamped_size.y >= max_bounds.y) {
		clamped_size.x = max_bounds.y / viewport_ratio;
		clamped_size.y = max_bounds.y;
	}

	if (clamped_size.x >= max_bounds.x) {
		clamped_size.x = max_bounds.x;
		clamped_size.y = max_bounds.x * viewport_ratio;
	}

	// Clamp the min size based on if it's portrait or landscape. Portrait min
	// size should be based on it's height. Landscape min size is based on it's
	// width. Applying min width to a portrait size would make it too big.
	bool is_portrait = viewport_ratio > 1;

	if (is_portrait && clamped_size.y <= MIN_PANEL_SIZE * EDSCALE) {
		clamped_size.x = MIN_PANEL_SIZE / viewport_ratio;
		clamped_size.y = MIN_PANEL_SIZE;
		clamped_size = clamped_size * EDSCALE;
	}

	if (!is_portrait && clamped_size.x <= MIN_PANEL_SIZE * EDSCALE) {
		clamped_size.x = MIN_PANEL_SIZE;
		clamped_size.y = MIN_PANEL_SIZE * viewport_ratio;
		clamped_size = clamped_size * EDSCALE;
	}

	// Round down to avoid sub-pixel artifacts, mainly seen around the margins.
	return clamped_size.floor();
}

void PIPCameraPreview::_on_animate_into_place_finished(Vector2 final_position) {
	set_position(final_position);
	state = INTERACTION_STATE_NONE;
}

void PIPCameraPreview::_on_resize_handle_button_down() {
	if (state != INTERACTION_STATE_NONE) {
		return;
	}

	state = INTERACTION_STATE_RESIZE;
	initial_mouse_position = get_global_mouse_position();
	initial_panel_size = get_size();
}

void PIPCameraPreview::_on_resize_handle_button_up() {
	state = INTERACTION_STATE_NONE;
}

void PIPCameraPreview::_on_drag_handle_button_down() {
	if (state != INTERACTION_STATE_NONE) {
		return;
	}

	state = INTERACTION_STATE_DRAG;
	initial_mouse_position = get_global_mouse_position();
	initial_panel_position = get_global_position();
}

void PIPCameraPreview::_on_drag_handle_button_up() {
	if (state != INTERACTION_STATE_DRAG) {
		return;
	}

	state = INTERACTION_STATE_START_ANIMATE_INTO_PLACE;
}
