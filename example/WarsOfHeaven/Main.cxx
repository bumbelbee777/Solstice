#include "WarsOfHeavenSmoke.hxx"
#include <Core/Debug/Debug.hxx>
#include <iostream>
#include <exception>

int main() {
    try {
        Solstice::WarsOfHeaven::WarsOfHeavenSmoke game;
        return game.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
