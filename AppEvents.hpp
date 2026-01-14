#pragma once

#include "app.hpp"
#include <atomic>

// Globální èítaè projektilù - definován v AppEvents.cpp
extern std::atomic<int> g_projectile_counter;