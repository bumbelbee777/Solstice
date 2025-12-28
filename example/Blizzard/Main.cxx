#include "BlizzardGame.hxx"
#include <Core/Debug.hxx>
#include <iostream>
#include <exception>

int main() {
    try {
        Solstice::Blizzard::BlizzardGame game;
        // Window will be created in Initialize() via InitializeWindow()
        return game.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}

