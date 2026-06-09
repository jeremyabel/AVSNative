#pragma once

#include "Effect.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Registry
{
public:

    void Register(const std::string& Name, std::function<std::unique_ptr<Effect>()> Factory);

    std::unique_ptr<Effect> Create(const std::string& Name) const;
    std::vector<std::string> Names() const;

private:

    std::map<std::string, std::function<std::unique_ptr<Effect>()>> Factories;
};
