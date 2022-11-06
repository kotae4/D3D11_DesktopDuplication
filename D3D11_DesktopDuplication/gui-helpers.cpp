#include "gui-helpers.h"
#include <string>


bool CustomListBoxInt(const char* label, int* value, const std::vector<std::string> list, float width, ImGuiComboFlags flags) {
	auto comboLabel = "##" + std::string(label);
	auto leftArrow = "##" + std::string(label) + "Left";
	auto rightArrow = "##" + std::string(label) + "Right";

	ImGuiStyle& style = ImGui::GetStyle();
	float spacing = style.ItemInnerSpacing.x;
	ImGui::PushItemWidth(width);
	bool response = ImGui::BeginCombo(comboLabel.c_str(), (*value >= 0 ? list.at(*value).c_str() : nullptr), ImGuiComboFlags_NoArrowButton | flags);
	if (response) {
		response = false;
		for (size_t i = 0; i < list.size(); i++) {
			bool is_selected = (*value == i);
			if (ImGui::Selectable(list.at(i).c_str(), is_selected)) {
				*value = (int)i;
				response = true;
			}
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::PopItemWidth();
	ImGui::SameLine(0, spacing);

	const bool LeftResponse = ImGui::ArrowButton(leftArrow.c_str(), ImGuiDir_Left);
	if (LeftResponse) {
		*value -= 1;
		if (*value < 0) *value = int(list.size() - 1);
		return LeftResponse;
	}
	ImGui::SameLine(0, spacing);
	const bool RightResponse = ImGui::ArrowButton(rightArrow.c_str(), ImGuiDir_Right);
	if (RightResponse) {
		*value += 1;
		if (*value > (int)(list.size() - 1)) *value = 0;
		return RightResponse;
	}
	ImGui::SameLine(0, spacing);
	ImGui::Text(label);

	return response;
}