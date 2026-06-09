#include "engine/Engine.h"
#include "ui/App.h"

int main(int argc, char* argv[])
{
    Engine engine;
    App app(engine);
    app.Run(argc > 1 ? argv[1] : nullptr);
    return 0;
}
