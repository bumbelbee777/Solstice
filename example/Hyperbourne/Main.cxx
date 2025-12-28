#include "HyperbourneGame.hxx"
#include <Core/Debug.hxx>
#include <iostream>
#include <exception>

int main() {
    try {
        Solstice::Hyperbourne::HyperbourneGame game;
        return game.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}

