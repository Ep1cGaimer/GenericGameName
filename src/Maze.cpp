#include "Maze.h"
#include <stack>
#include <algorithm>
#include <random>
#include <ctime>

const int DX[] = { 0, 1, 0, -1 };
const int DY[] = { -1, 0, 1, 0 };
const Direction OPPOSITE[] = { SOUTH, WEST, NORTH, EAST };

MazeGenerator::MazeGenerator(int w, int h) : width(w), height(h) {
    grid.resize(width, std::vector<Cell>(height));
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            grid[x][y].x = x;
            grid[x][y].y = y;
        }
    }
}

void MazeGenerator::generate() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::stack<Cell*> stack;

    Cell* start = &grid[0][0];
    start->visited = true;
    stack.push(start);

    while (!stack.empty()) {
        Cell* current = stack.top();
        std::vector<int> neighbors;

        for (int d = 0; d < 4; d++) {
            int nx = current->x + DX[d];
            int ny = current->y + DY[d];

            if (nx >= 0 && nx < width && ny >= 0 && ny < height && !grid[nx][ny].visited) {
                neighbors.push_back(d);
            }
        }

        if (!neighbors.empty()) {
            std::uniform_int_distribution<int> dist(0, neighbors.size() - 1);
            int dir = neighbors[dist(rng)];

            int nx = current->x + DX[dir];
            int ny = current->y + DY[dir];

            current->walls[dir] = false;
            grid[nx][ny].walls[OPPOSITE[dir]] = false;

            grid[nx][ny].visited = true;
            stack.push(&grid[nx][ny]);
        } else {
            stack.pop();
        }
    }
}

void MazeGenerator::addLoops(float chance) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d];
                int ny = y + DY[d];

                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    if (grid[x][y].walls[d] && dist(rng) < chance) {
                        grid[x][y].walls[d] = false;
                        grid[nx][ny].walls[OPPOSITE[d]] = false;
                    }
                }
            }
        }
    }
}

void MazeGenerator::removeDeadEnds(int passes) {
    for (int p = 0; p < passes; p++) {
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                int openWalls = 0;
                int lastOpenDir = -1;
                for (int d = 0; d < 4; d++) {
                    if (!grid[x][y].walls[d]) {
                        openWalls++;
                        lastOpenDir = d;
                    }
                }

                if (openWalls == 1) {
                    grid[x][y].walls[lastOpenDir] = true;
                    int nx = x + DX[lastOpenDir];
                    int ny = y + DY[lastOpenDir];
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        grid[nx][ny].walls[OPPOSITE[lastOpenDir]] = true;
                    }
                }
            }
        }
    }
}

bool MazeGenerator::hasWall(int x, int y, int direction) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return true;
    return grid[x][y].walls[direction];
}

bool MazeGenerator::isPassable(int x1, int y1, int x2, int y2) const {
    if (x1 < 0 || x1 >= width || y1 < 0 || y1 >= height) return false;
    if (x2 < 0 || x2 >= width || y2 < 0 || y2 >= height) return false;

    for (int d = 0; d < 4; d++) {
        if (x1 + DX[d] == x2 && y1 + DY[d] == y2) {
            return !grid[x1][y1].walls[d];
        }
    }
    return false;
}
