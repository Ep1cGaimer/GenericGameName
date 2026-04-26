#include "Collision.h"
#include <cmath>

MazeCollision::MazeCollision(const MazeGenerator& maze, float ts, float pr)
    : tileSize(ts), playerRadius(pr)
{
    // Build the same tile map as the renderer
    tileW = 2 * maze.width + 1;
    tileH = 2 * maze.height + 1;
    tileMap.resize(tileW, std::vector<bool>(tileH, true)); // default solid

    for (int cx = 0; cx < maze.width; cx++) {
        for (int cy = 0; cy < maze.height; cy++) {
            tileMap[2 * cx + 1][2 * cy + 1] = false; // cell interior

            if (!maze.grid[cx][cy].walls[NORTH]) {
                tileMap[2 * cx + 1][2 * cy] = false;
            }
            if (!maze.grid[cx][cy].walls[WEST]) {
                tileMap[2 * cx][2 * cy + 1] = false;
            }
            if (cy == maze.height - 1 && !maze.grid[cx][cy].walls[SOUTH]) {
                tileMap[2 * cx + 1][2 * cy + 2] = false;
            }
            if (cx == maze.width - 1 && !maze.grid[cx][cy].walls[EAST]) {
                tileMap[2 * cx + 2][2 * cy + 1] = false;
            }
        }
    }
}

bool MazeCollision::isTileSolid(int tx, int tz) const {
    if (tx < 0 || tx >= tileW || tz < 0 || tz >= tileH) return true;
    return tileMap[tx][tz];
}

bool MazeCollision::isBlocked(glm::vec3 pos) {
    // Check all 4 corners of the player's bounding circle against the tile map
    float r = playerRadius;
    
    // Check the tile at each corner of the player's AABB
    for (float dx = -r; dx <= r; dx += 2 * r) {
        for (float dz = -r; dz <= r; dz += 2 * r) {
            float px = pos.x + dx;
            float pz = pos.z + dz;
            
            int tx = (int)std::floor(px / tileSize);
            int tz = (int)std::floor(pz / tileSize);
            
            if (isTileSolid(tx, tz)) return true;
        }
    }
    
    return false;
}

glm::vec3 MazeCollision::resolveMovement(glm::vec3 currentPos, glm::vec3 desiredPos) {
    glm::vec3 result = currentPos;

    // Try X axis independently (enables wall sliding)
    glm::vec3 tryX = currentPos;
    tryX.x = desiredPos.x;
    if (!isBlocked(tryX)) {
        result.x = desiredPos.x;
    }

    // Try Z axis independently
    glm::vec3 tryZ = result;
    tryZ.z = desiredPos.z;
    if (!isBlocked(tryZ)) {
        result.z = desiredPos.z;
    }

    // Lock Y
    result.y = currentPos.y;

    return result;
}
