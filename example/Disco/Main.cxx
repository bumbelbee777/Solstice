#include <Solstice.hxx>
#include <UI/Core/Window.hxx>
#include "DiscoGame.hxx"

using namespace Solstice;

int main(int argc, char** argv) {
    // Correct global initialization
    Solstice::Initialize();
    
    {
        // Window usually initializes SDL, so we need it early
        auto window = std::make_unique<UI::Window>(1280, 720, "Solstice - Disco Demo");
        
        DiscoDemo::DiscoGame game;
        game.SetWindow(std::move(window));
        
        game.Run();
    }

    Solstice::Shutdown();
    return 0;
}
