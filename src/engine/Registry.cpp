#include "Registry.h"

void Registry::Register(const std::string& Name, std::function<std::unique_ptr<Effect>()> Factory)
{
    Factories[Name] = std::move(Factory);
}

std::unique_ptr<Effect> Registry::Create(const std::string& Name) const
{
    auto Found = Factories.find(Name);
    
    if (Found == Factories.end())
        return nullptr;

    return Found->second();
}

std::vector<std::string> Registry::Names() const
{
    std::vector<std::string> Result;
    Result.reserve(Factories.size());

    for (const auto& [Name, _] : Factories)
    {
        Result.push_back(Name);
    }

    return Result;
}
