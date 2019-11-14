#include <ui/TextField.hpp>

namespace rack {
namespace ui {


TextField::TextField() {
	box.size.y = BND_WIDGET_HEIGHT;
}

void TextField::draw(const DrawArgs& args) {
	nvgScissor(args.vg, RECT_ARGS(args.clipBox));

	BNDwidgetState state;
	if (this == APP->event->selectedWidget)
		state = BND_ACTIVE;
	else if (this == APP->event->hoveredWidget)
		state = BND_HOVER;
	else
		state = BND_DEFAULT;

	int begin = std::min(cursor, selection);
	int end = std::max(cursor, selection);
	bndTextField(args.vg, 0.0, 0.0, box.size.x, box.size.y, BND_CORNER_NONE, state, -1, text.c_str(), begin, end);
	// Draw placeholder text
	if (text.empty() && state != BND_ACTIVE) {
		bndIconLabelCaret(args.vg, 0.0, 0.0, box.size.x, box.size.y, -1, bndGetTheme()->textFieldTheme.itemColor, 13, placeholder.c_str(), bndGetTheme()->textFieldTheme.itemColor, 0, -1);
	}

	nvgResetScissor(args.vg);
}

void TextField::onDragHover(const event::DragHover& e) {
	OpaqueWidget::onDragHover(e);

	if (e.origin == this) {
		int pos = getTextPosition(e.pos);
		cursor = pos;
	}
}

void TextField::onButton(const event::Button& e) {
	OpaqueWidget::onButton(e);

	if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
		cursor = selection = getTextPosition(e.pos);
	}
}

void TextField::onSelectText(const event::SelectText& e) {
	if (e.codepoint < 128) {
		std::string newText(1, (char) e.codepoint);
		insertText(newText);
	}
	e.consume(this);
}

void TextField::onSelectKey(const event::SelectKey& e) {
	if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
		if (e.key == GLFW_KEY_BACKSPACE && (e.mods & RACK_MOD_MASK) == 0) {
			if (cursor == selection) {
				cursor--;
				if (cursor >= 0) {
					text.erase(cursor, 1);
					event::Change eChange;
					onChange(eChange);
				}
				selection = cursor;
			}
			else {
				int begin = std::min(cursor, selection);
				text.erase(begin, std::abs(selection - cursor));
				event::Change eChange;
				onChange(eChange);
				cursor = selection = begin;
			}
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_DELETE && (e.mods & RACK_MOD_MASK) == 0) {
			if (cursor == selection) {
				text.erase(cursor, 1);
				event::Change eChange;
				onChange(eChange);
			}
			else {
				int begin = std::min(cursor, selection);
				text.erase(begin, std::abs(selection - cursor));
				event::Change eChange;
				onChange(eChange);
				cursor = selection = begin;
			}
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_LEFT && (e.mods & RACK_MOD_MASK) == 0) {
			if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				while (--cursor > 0) {
					if (text[cursor] == ' ')
						break;
				}
			}
			else {
				cursor--;
			}
			if ((e.mods & RACK_MOD_MASK) == 0) {
				selection = cursor;
			}
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_RIGHT && (e.mods & RACK_MOD_MASK) == 0) {
			if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				while (++cursor < (int) text.size()) {
					if (text[cursor] == ' ')
						break;
				}
			}
			else {
				cursor++;
			}
			if ((e.mods & RACK_MOD_MASK) == 0) {
				selection = cursor;
			}
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_HOME && (e.mods & RACK_MOD_MASK) == 0) {
			selection = cursor = 0;
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_END && (e.mods & RACK_MOD_MASK) == 0) {
			selection = cursor = text.size();
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_V && (e.mods & RACK_MOD_MASK) == 0) {
			if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				const char* newText = glfwGetClipboardString(APP->window->win);
				if (newText)
					insertText(newText);
			}
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_X && (e.mods & RACK_MOD_MASK) == 0) {
			if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				if (cursor != selection) {
					int begin = std::min(cursor, selection);
					std::string selectedText = text.substr(begin, std::abs(selection - cursor));
					glfwSetClipboardString(APP->window->win, selectedText.c_str());
					insertText("");
				}
			}
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_C && (e.mods & RACK_MOD_MASK) == 0) {
			if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				if (cursor != selection) {
					int begin = std::min(cursor, selection);
					std::string selectedText = text.substr(begin, std::abs(selection - cursor));
					glfwSetClipboardString(APP->window->win, selectedText.c_str());
				}
			}
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_A && (e.mods & RACK_MOD_MASK) == 0) {
			if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				selectAll();
			}
			e.consume(this);
		}
		else if (e.key == GLFW_KEY_ENTER && (e.mods & RACK_MOD_MASK) == 0) {
			if (multiline) {
				insertText("\n");
			}
			else {
				event::Action eAction;
				onAction(eAction);
			}
			e.consume(this);
		}
		// Consume all printable keys
		else if ((GLFW_KEY_SPACE <= e.key && e.key <= GLFW_KEY_WORLD_2) || (GLFW_KEY_KP_0 <= e.key && e.key <= GLFW_KEY_KP_EQUAL)) {
			e.consume(this);
		}

		cursor = math::clamp(cursor, 0, (int) text.size());
		selection = math::clamp(selection, 0, (int) text.size());
	}
}

void TextField::insertText(std::string text) {
	if (cursor != selection) {
		int begin = std::min(cursor, selection);
		this->text.erase(begin, std::abs(selection - cursor));
		cursor = selection = begin;
	}
	this->text.insert(cursor, text);
	cursor += text.size();
	selection = cursor;
	event::Change eChange;
	onChange(eChange);
}

void TextField::setText(std::string text) {
	if (this->text != text) {
		this->text = text;
		// event::Change
		event::Change eChange;
		onChange(eChange);
	}
	selection = cursor = text.size();
}

void TextField::selectAll() {
	cursor = text.size();
	selection = 0;
}

int TextField::getTextPosition(math::Vec mousePos) {
	return bndTextFieldTextPosition(APP->window->vg, 0.0, 0.0, box.size.x, box.size.y, -1, text.c_str(), mousePos.x, mousePos.y);
}


} // namespace ui
} // namespace rack
