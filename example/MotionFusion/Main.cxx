#include "MotionFusionGame.hxx"
#include <iostream>
#include <exception>

int main() {
    try {
        Solstice::MotionFusion::MotionFusionGame game;
        return game.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}
