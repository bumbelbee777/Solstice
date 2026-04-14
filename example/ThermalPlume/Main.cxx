#include "ThermalPlumeGame.hxx"
#include <Core/Debug/Debug.hxx>
#include <iostream>
#include <exception>

int main() {
    try {
        Solstice::ThermalPlume::ThermalPlumeGame game;
        return game.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}
