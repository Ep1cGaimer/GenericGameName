#ifndef GAME_STATE_H
#define GAME_STATE_H

enum class GameState {
    MENU,
    PLAYING,
    LOSE
};

class GameManager {
public:
    GameState currentState = GameState::MENU;

    void update(float deltaTime) {
        // Transition to PLAYING for testing
        if (currentState == GameState::MENU) {
            currentState = GameState::PLAYING;
        }
    }
};

#endif
