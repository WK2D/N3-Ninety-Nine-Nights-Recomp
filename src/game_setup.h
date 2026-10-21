#pragma once
#include <rex/rex_app.h>

std::optional<rex::PathConfig> ResolveGameData(const rex::PathConfig& defaults);
void ConfigureGameDataPaths(rex::PathConfig& paths);
