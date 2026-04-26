#ifndef MAZE_H
#define MAZE_H

#include <vector>

enum Direction {
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
};

struct Cell {
    int x, y;
    bool visited = false;
    bool walls[4] = {true, true, true, true}; // N, E, S, W
};

class MazeGenerator {
public:
    int width, height;
    std::vector<std::vector<Cell>> grid;

    MazeGenerator(int w, int h);
    void generate();
    void addLoops(float chance = 0.05f);
    void removeDeadEnds(int passes = 2);
    bool hasWall(int x, int y, int direction) const;
    bool isPassable(int x1, int y1, int x2, int y2) const;
};

#endif
