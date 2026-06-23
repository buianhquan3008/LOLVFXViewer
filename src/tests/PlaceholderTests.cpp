#include "bin/BinAst.h"
#include "bin/BinParser.h"
#include "bin/BinToVfxGraph.h"

#include <iostream>
#include <string>

namespace
{
bool Expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

const lol::BinObject* RequireObject(const lol::BinValue* value, const std::string& message)
{
    if (value == nullptr)
    {
        std::cerr << message << "\n";
        return nullptr;
    }

    const auto object = value->AsObject();
    if (object == nullptr || !(*object))
    {
        std::cerr << message << "\n";
        return nullptr;
    }

    return object->get();
}
}

int main()
{
    lol::BinParser parser;
    lol::BinParseError error;

    {
        lol::BinObject output;
        if (parser.Parse("", output, error))
        {
            std::cerr << "Expected empty file to fail\n";
            return 1;
        }
        if (!Expect(error.location.line == 1 && error.location.column == 1, "Empty file should fail at 1:1"))
        {
            return 1;
        }
    }

    {
        const std::string source = R"(
Root {
    enabled: true;
    count: 3;
    ratio: 1.5;
    name: "spark";
}
)";
        lol::BinObject output;
        if (!parser.Parse(source, output, error))
        {
            std::cerr << "Simple object parse failed: " << error.message << "\n";
            return 1;
        }
        if (!Expect(output.typeName == "Root", "Root object type mismatch")) return 1;
        if (!Expect(output.FindField("enabled") != nullptr && *output.FindField("enabled")->AsBool(), "Bool field mismatch")) return 1;
        if (!Expect(output.FindField("count") != nullptr && *output.FindField("count")->AsInt() == 3, "Integer field mismatch")) return 1;
        if (!Expect(output.FindField("ratio") != nullptr && *output.FindField("ratio")->AsDouble() == 1.5, "Float field mismatch")) return 1;
        if (!Expect(output.FindField("name") != nullptr && *output.FindField("name")->AsString() == "spark", "String field mismatch")) return 1;
    }

    {
        const std::string source = R"(
Root {
    nested: Child {
        value: 42;
    };
}
)";
        lol::BinObject output;
        if (!parser.Parse(source, output, error))
        {
            std::cerr << "Nested object parse failed: " << error.message << "\n";
            return 1;
        }
        const lol::BinObject* child = RequireObject(output.FindField("nested"), "Nested object missing");
        if (child == nullptr) return 1;
        if (!Expect(child->typeName == "Child", "Nested object type mismatch")) return 1;
        if (!Expect(child->FindField("value") != nullptr && *child->FindField("value")->AsInt() == 42, "Nested field mismatch")) return 1;
    }

    {
        const std::string source = R"(
Root {
    emitters: [
        Emitter { name: "A"; },
        Emitter { name: "B"; }
    ];
}
)";
        lol::BinObject output;
        if (!parser.Parse(source, output, error))
        {
            std::cerr << "List parse failed: " << error.message << "\n";
            return 1;
        }
        const lol::BinValue* emitters = output.FindField("emitters");
        if (!Expect(emitters != nullptr && emitters->IsList(), "Emitter list missing")) return 1;
        const lol::BinList* list = emitters->AsList();
        if (!Expect(list != nullptr && list->size() == 2, "Emitter list size mismatch")) return 1;
        const lol::BinObject* firstEmitter = RequireObject(&(*list)[0], "First emitter missing");
        if (firstEmitter == nullptr) return 1;
        if (!Expect(firstEmitter->typeName == "Emitter", "First emitter type mismatch")) return 1;
    }

    {
        const std::string source = R"(
Root {
    unknownHash = Fx/Fireball;
}
)";
        lol::BinObject output;
        if (!parser.Parse(source, output, error))
        {
            std::cerr << "Unknown/bare identifier parse failed: " << error.message << "\n";
            return 1;
        }
        if (!Expect(output.FindField("unknownHash") != nullptr && *output.FindField("unknownHash")->AsString() == "Fx/Fireball", "Bare identifier preservation failed")) return 1;
    }

    {
        const std::string source = R"(
Root {
    broken:
}
)";
        lol::BinObject output;
        if (parser.Parse(source, output, error))
        {
            std::cerr << "Expected invalid syntax to fail\n";
            return 1;
        }
        if (!Expect(!error.message.empty() && error.location.line > 0 && error.location.column > 0, "Invalid syntax should report a location")) return 1;
    }

    {
        const std::string source = R"(
VfxSystemDefinitionData {
    effectName: "Demo";
    emitters: [
        VfxEmitterDefinitionData {
            name: "Core";
            spawnRate: 32;
            particle: VfxParticleDefinitionData {
                lifetime: 1.25;
                texture: "fx/fire.png";
            };
        }
    ];
    curves: [
        Curve {
            name: "Alpha";
            points: [ Point { x: 0.0; y: 1.0; }, Point { x: 1.0; y: 0.0; } ];
        }
    ];
}
)";
        lol::BinObject output;
        if (!parser.Parse(source, output, error))
        {
            std::cerr << "VFX sample parse failed: " << error.message << " at " << error.location.line << ":" << error.location.column << "\n";
            return 1;
        }
        if (!Expect(output.typeName == "VfxSystemDefinitionData", "VFX root type mismatch")) return 1;
        const lol::BinValue* curves = output.FindField("curves");
        if (!Expect(curves != nullptr && curves->IsList(), "Curve list missing")) return 1;

        std::string warnings;
        const std::optional<lol::VfxGraph> graph = lol::BinToVfxGraph::Convert(output, "sample_vfx.bin.txt", warnings);
        if (!Expect(graph.has_value(), "BIN to VFX graph conversion failed")) return 1;
        if (!Expect(graph->name == "Demo", "VFX graph name mismatch")) return 1;
        if (!Expect(graph->emitters.size() == 1, "VFX graph emitter count mismatch")) return 1;
        if (!Expect(graph->emitters[0].name == "Core", "Mapped emitter name mismatch")) return 1;
        if (!Expect(graph->emitters[0].materialName.empty() || graph->emitters[0].materialName == "FireMaterial", "Emitter material mapping should remain stable")) return 1;
        if (!Expect(graph->curves.size() == 1, "VFX graph curve count mismatch")) return 1;
    }

    std::cout << "LoLAssetStudioTests passed\n";
    return 0;
}
