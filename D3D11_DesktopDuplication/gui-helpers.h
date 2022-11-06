#pragma once

#include <vector>
#include <string>
#include "imgui/imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui_internal.h"


static inline ImVec2 operator+(const ImVec2& lhs, const float scalar) { return ImVec2(lhs.x + scalar, lhs.y + scalar); }
static inline ImVec2 operator-(const ImVec2& lhs, const float scalar) { return ImVec2(lhs.x - scalar, lhs.y - scalar); }
static inline ImVec2& operator+=(ImVec2& lhs, const float scalar) { lhs.x += scalar; lhs.y += scalar; return lhs; }
static inline ImVec2& operator-=(ImVec2& lhs, const float scalar) { lhs.x -= scalar; lhs.y -= scalar; return lhs; }

bool CustomListBoxInt(const char* label, int* value, const std::vector<std::string> list, float width = 225.f, ImGuiComboFlags flags = ImGuiComboFlags_None);