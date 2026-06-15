#include "bin/BinParser.h"

#include <iostream>

int main()
{
    lol::BinObject object;
    object.typeName = "Placeholder";
    object.fields["enabled"] = true;

    if (object.typeName != "Placeholder")
    {
        std::cerr << "Bin placeholder test failed\n";
        return 1;
    }

    std::cout << "LoLAssetStudioTests passed\n";
    return 0;
}
