#ifndef COLLISION_H
#define COLLISION_H

#include <glm/glm.hpp>
#include "Maze.h"

class MazeCollision {
public:
    MazeCollision(const MazeGenerator& maze, float tileSize, float playerRadius);
    glm::vec3 resolveMovement(glm::vec3 currentPos, glm::vec3 desiredPos);

    // Expose tile map for external use (e.g. enemy AI)
    bool isTileSolid(int tx, int tz) const;
    int getTileWidth() const { return tileW; }
    int getTileHeight() const { return tileH; }

private:
    float tileSize;
    float playerRadius;
    int tileW, tileH;
    std::vector<std::vector<bool>> tileMap;

    bool isBlocked(glm::vec3 pos);
};

#endif
