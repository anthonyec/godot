#ifndef PIP_CAMERA_PREVIEW_PLUGIN_H
#define PIP_CAMERA_PREVIEW_PLUGIN_H

#include "editor/editor_plugin.h"

#include "scene/main/viewport.h"
#include "scene/gui/panel.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/button.h"
#include "scene/gui/texture_rect.h"

class PIPCameraPreview : public Panel {
	GDCLASS(PIPCameraPreview, Panel);

	public:
		enum PinnedEdge {
			PINNED_EDGE_LEFT,
			PINNED_EDGE_RIGHT
		};

	private:
		enum CAMERA_TYPE {
			CAMERA_TYPE_2D,
			CAMERA_TYPE_3D
		};

		enum InteractionState {
			INTERACTION_STATE_NONE,
			INTERACTION_STATE_RESIZE,
			INTERACTION_STATE_DRAG,
			INTERACTION_STATE_START_ANIMATE_INTO_PLACE,
			INTERACTION_STATE_ANIMATE_INTO_PLACE
		};

		Control *container = nullptr;

		Panel *placeholder = nullptr;
		SubViewport *sub_viewport = nullptr;
		MarginContainer *viewport_texture_container = nullptr;
		TextureRect *viewport_texture = nullptr;
		Button *drag_handle = nullptr;
		MarginContainer *overlay_margin_container = nullptr;
		Control *overlay_container = nullptr;
		Button *resize_left_handle = nullptr;
		Button *resize_right_handle = nullptr;
		Button *pin_button = nullptr;

		InteractionState state;
		PinnedEdge pinned_edge;
		bool show_controls;
		Vector2 initial_mouse_position;
		Vector2 initial_panel_position;
		Vector2 initial_panel_size;

	protected:
		static void _bind_methods();
		void _notification(int p_what);
		Vector2 _get_pinned_position(PinnedEdge pinned_edge);
		Vector2 _get_clamped_position(Vector2 desired_position);
		Vector2 _get_clamped_size(Vector2 desired_size);
		Vector2 _get_project_window_size();
		float _get_project_window_ratio();
		void _on_animate_into_place_finished(Vector2 final_position);
		void _on_resize_handle_button_down();
		void _on_resize_handle_button_up();
		void _on_drag_handle_button_down();
		void _on_drag_handle_button_up();

	public:
		PIPCameraPreview(Control *container);
		~PIPCameraPreview();
		PinnedEdge get_pinned_edge() const { return pinned_edge; };
};

#endif // PIP_CAMERA_PREVIEW_PLUGIN_H
