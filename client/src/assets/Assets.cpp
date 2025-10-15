#include "Screens.hpp"
#include <raylib.h>
#include <vector>
#include <string>

namespace client { namespace ui {

std::string Screens::findSpritePath(const char* name) const {
    std::vector<std::string> candidates;
    candidates.emplace_back(std::string("client/sprites/") + name);
    candidates.emplace_back(std::string("../client/sprites/") + name);
    candidates.emplace_back(std::string("../../client/sprites/") + name);
    candidates.emplace_back(std::string("../../../client/sprites/") + name);
    for (const auto& c : candidates) {
        if (FileExists(c.c_str())) return c;
    }
    return {};
}

bool Screens::assetsAvailable() const {
    std::string p1 = findSpritePath("r-typesheet42.gif");
    std::string p2 = findSpritePath("r-typesheet19.gif");
    return !p1.empty() && !p2.empty();
}

void Screens::loadSprites() {
    if (_sheetLoaded) return;
    std::string path = findSpritePath("r-typesheet42.gif");
    if (path.empty()) { logMessage("Spritesheet r-typesheet42.gif not found.", "WARN"); return; }
    _sheet = LoadTexture(path.c_str());
    if (_sheet.id == 0) { logMessage("Failed to load spritesheet texture.", "ERROR"); return; }
    _sheetLoaded = true;
    _frameW = (float)_sheet.width / (float)_sheetCols;
    _frameH = (float)_sheet.height / (float)_sheetRows;
    logMessage("Spritesheet loaded: " + std::to_string(_sheet.width) + "x" + std::to_string(_sheet.height) +
               ", frame " + std::to_string((int)_frameW) + "x" + std::to_string((int)_frameH), "INFO");
}

void Screens::loadEnemySprites() {
    if (_enemyLoaded) return;
    std::string path = findSpritePath("r-typesheet19.gif");
    if (path.empty()) { logMessage("Enemy spritesheet r-typesheet19.gif not found.", "WARN"); return; }
    _enemySheet = LoadTexture(path.c_str());
    if (_enemySheet.id == 0) { logMessage("Failed to load enemy spritesheet texture.", "ERROR"); return; }
    _enemyLoaded = true;
    _enemyCols = 7;
    _enemyRows = 3;
    _enemyFrameW = (float)_enemySheet.width / (float)_enemyCols;
    _enemyFrameH = (float)_enemySheet.height / (float)_enemyRows;
    logMessage(
        "Enemy sheet loaded: " + std::to_string(_enemySheet.width) + "x" + std::to_string(_enemySheet.height) +
        ", grid " + std::to_string(_enemyCols) + "x" + std::to_string(_enemyRows) +
        ", frame " + std::to_string((int)_enemyFrameW) + "x" + std::to_string((int)_enemyFrameH), "INFO");
}

void Screens::unloadGraphics() {
    if (_sheetLoaded) { UnloadTexture(_sheet); _sheetLoaded = false; }
    if (_enemyLoaded) { UnloadTexture(_enemySheet); _enemyLoaded = false; }
}

Screens::~Screens() {
    if (IsWindowReady()) {
        if (_sheetLoaded) { UnloadTexture(_sheet); _sheetLoaded = false; }
        if (_enemyLoaded) { UnloadTexture(_enemySheet); _enemyLoaded = false; }
    }
}

} } // namespace client::ui
