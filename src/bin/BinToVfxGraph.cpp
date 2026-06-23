#include "bin/BinToVfxGraph.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace lol
{
namespace
{
std::string Lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ContainsInsensitive(const std::string& text, const std::string& needle)
{
    return Lowercase(text).find(Lowercase(needle)) != std::string::npos;
}

std::string ResolveRelativePath(const std::filesystem::path& sourcePath, const std::string& candidate)
{
    if (candidate.empty())
    {
        return {};
    }

    std::filesystem::path resolved(candidate);
    if (resolved.is_absolute() && std::filesystem::exists(resolved))
    {
        return resolved.lexically_normal().string();
    }

    resolved = sourcePath.parent_path() / resolved;
    if (std::filesystem::exists(resolved))
    {
        return resolved.lexically_normal().string();
    }

    return candidate;
}

std::optional<float> ValueToFloat(const BinValue& value)
{
    if (const int64_t* integerValue = value.AsInt())
    {
        return static_cast<float>(*integerValue);
    }
    if (const double* floatValue = value.AsDouble())
    {
        return static_cast<float>(*floatValue);
    }
    if (const std::string* stringValue = value.AsString())
    {
        try
        {
            return std::stof(*stringValue);
        }
        catch (...)
        {
        }
    }
    return std::nullopt;
}

std::optional<bool> ValueToBool(const BinValue& value)
{
    if (const bool* boolValue = value.AsBool())
    {
        return *boolValue;
    }
    if (const int64_t* integerValue = value.AsInt())
    {
        return *integerValue != 0;
    }
    if (const std::string* stringValue = value.AsString())
    {
        const std::string lowered = Lowercase(*stringValue);
        if (lowered == "true" || lowered == "1")
        {
            return true;
        }
        if (lowered == "false" || lowered == "0")
        {
            return false;
        }
    }
    return std::nullopt;
}

std::optional<std::string> ValueToString(const BinValue& value)
{
    if (const std::string* stringValue = value.AsString())
    {
        return *stringValue;
    }
    if (const int64_t* integerValue = value.AsInt())
    {
        return std::to_string(*integerValue);
    }
    if (const double* floatValue = value.AsDouble())
    {
        return std::to_string(*floatValue);
    }
    if (const bool* boolValue = value.AsBool())
    {
        return *boolValue ? "true" : "false";
    }
    return std::nullopt;
}

std::optional<glm::vec3> ValueToVec3(const BinValue& value)
{
    if (const BinList* list = value.AsList())
    {
        if (list->size() >= 3)
        {
            const std::optional<float> x = ValueToFloat((*list)[0]);
            const std::optional<float> y = ValueToFloat((*list)[1]);
            const std::optional<float> z = ValueToFloat((*list)[2]);
            if (x && y && z)
            {
                return glm::vec3(*x, *y, *z);
            }
        }
    }

    if (const BinObjectPtr* objectPointer = value.AsObject())
    {
        if (*objectPointer == nullptr)
        {
            return std::nullopt;
        }

        const BinObject& object = **objectPointer;
        const BinValue* xValue = object.FindField("x");
        const BinValue* yValue = object.FindField("y");
        const BinValue* zValue = object.FindField("z");
        if (xValue && yValue && zValue)
        {
            const std::optional<float> x = ValueToFloat(*xValue);
            const std::optional<float> y = ValueToFloat(*yValue);
            const std::optional<float> z = ValueToFloat(*zValue);
            if (x && y && z)
            {
                return glm::vec3(*x, *y, *z);
            }
        }
    }

    return std::nullopt;
}

std::optional<glm::vec4> ValueToVec4(const BinValue& value)
{
    if (const BinList* list = value.AsList())
    {
        if (list->size() >= 4)
        {
            const std::optional<float> x = ValueToFloat((*list)[0]);
            const std::optional<float> y = ValueToFloat((*list)[1]);
            const std::optional<float> z = ValueToFloat((*list)[2]);
            const std::optional<float> w = ValueToFloat((*list)[3]);
            if (x && y && z && w)
            {
                return glm::vec4(*x, *y, *z, *w);
            }
        }
    }

    if (const BinObjectPtr* objectPointer = value.AsObject())
    {
        if (*objectPointer == nullptr)
        {
            return std::nullopt;
        }

        const BinObject& object = **objectPointer;
        const BinValue* rValue = object.FindField("r");
        const BinValue* gValue = object.FindField("g");
        const BinValue* bValue = object.FindField("b");
        const BinValue* aValue = object.FindField("a");
        if (rValue && gValue && bValue && aValue)
        {
            const std::optional<float> r = ValueToFloat(*rValue);
            const std::optional<float> g = ValueToFloat(*gValue);
            const std::optional<float> b = ValueToFloat(*bValue);
            const std::optional<float> a = ValueToFloat(*aValue);
            if (r && g && b && a)
            {
                return glm::vec4(*r, *g, *b, *a);
            }
        }

        const BinValue* xValue = object.FindField("x");
        const BinValue* yValue = object.FindField("y");
        const BinValue* zValue = object.FindField("z");
        const BinValue* wValue = object.FindField("w");
        if (xValue && yValue && zValue && wValue)
        {
            const std::optional<float> x = ValueToFloat(*xValue);
            const std::optional<float> y = ValueToFloat(*yValue);
            const std::optional<float> z = ValueToFloat(*zValue);
            const std::optional<float> w = ValueToFloat(*wValue);
            if (x && y && z && w)
            {
                return glm::vec4(*x, *y, *z, *w);
            }
        }
    }

    return std::nullopt;
}

const BinValue* FindFieldInsensitive(const BinObject& object, const std::initializer_list<const char*> candidates)
{
    for (const char* candidate : candidates)
    {
        if (const BinValue* exact = object.FindField(candidate))
        {
            return exact;
        }
    }

    for (const auto& [fieldName, fieldValue] : object.fields)
    {
        for (const char* candidate : candidates)
        {
            if (Lowercase(fieldName) == Lowercase(candidate))
            {
                return &fieldValue;
            }
        }
    }

    return nullptr;
}

std::vector<const BinObject*> CollectListObjectsByType(const BinValue* value, const std::initializer_list<const char*> typeNeedles)
{
    std::vector<const BinObject*> objects;
    if (value == nullptr)
    {
        return objects;
    }

    if (const BinList* list = value->AsList())
    {
        for (const BinValue& entry : *list)
        {
            if (const BinObjectPtr* objectPointer = entry.AsObject())
            {
                if (*objectPointer == nullptr)
                {
                    continue;
                }
                for (const char* needle : typeNeedles)
                {
                    if (ContainsInsensitive((*objectPointer)->typeName, needle))
                    {
                        objects.push_back(objectPointer->get());
                        break;
                    }
                }
            }
        }
    }
    return objects;
}

void CollectObjectsByTypeRecursive(const BinObject& object, const std::initializer_list<const char*> typeNeedles, std::vector<const BinObject*>& results)
{
    for (const char* needle : typeNeedles)
    {
        if (ContainsInsensitive(object.typeName, needle))
        {
            results.push_back(&object);
            break;
        }
    }

    for (const auto& field : object.orderedFields)
    {
        if (const BinObjectPtr* childObject = field.value.AsObject())
        {
            if (*childObject != nullptr)
            {
                CollectObjectsByTypeRecursive(**childObject, typeNeedles, results);
            }
        }
        else if (const BinList* list = field.value.AsList())
        {
            for (const BinValue& entry : *list)
            {
                if (const BinObjectPtr* listObject = entry.AsObject())
                {
                    if (*listObject != nullptr)
                    {
                        CollectObjectsByTypeRecursive(**listObject, typeNeedles, results);
                    }
                }
            }
        }
    }
}

std::vector<const BinObject*> CollectObjectsByTypeRecursive(const BinObject& object, const std::initializer_list<const char*> typeNeedles)
{
    std::vector<const BinObject*> results;
    CollectObjectsByTypeRecursive(object, typeNeedles, results);
    return results;
}

VfxEmitter DefaultEmitter()
{
    VfxEmitter emitter;
    emitter.name = "Default";
    emitter.spawnRate = 16.0f;
    emitter.lifetimeMin = 0.7f;
    emitter.lifetimeMax = 1.2f;
    emitter.velocityMin = glm::vec3(-0.2f, 0.8f, -0.2f);
    emitter.velocityMax = glm::vec3(0.2f, 1.5f, 0.2f);
    emitter.startColor = glm::vec4(1.0f, 0.75f, 0.2f, 1.0f);
    emitter.endColor = glm::vec4(1.0f, 0.1f, 0.0f, 0.0f);
    emitter.startSize = 0.35f;
    emitter.endSize = 0.08f;
    emitter.maxParticles = 256;
    emitter.additiveBlending = true;
    return emitter;
}

std::string GraphNameFromRoot(const BinObject& root, const std::filesystem::path& sourcePath)
{
    if (const BinValue* field = FindFieldInsensitive(root, {"effectName", "name", "mName"}))
    {
        if (const std::optional<std::string> value = ValueToString(*field))
        {
            return *value;
        }
    }
    return sourcePath.stem().string();
}

float GraphDurationFromRoot(const BinObject& root)
{
    if (const BinValue* field = FindFieldInsensitive(root, {"durationSeconds", "duration", "effectLifeTime"}))
    {
        if (const std::optional<float> value = ValueToFloat(*field))
        {
            return *value;
        }
    }
    return 5.0f;
}

bool GraphLoopFromRoot(const BinObject& root)
{
    if (const BinValue* field = FindFieldInsensitive(root, {"loop", "isLooping", "looping"}))
    {
        if (const std::optional<bool> value = ValueToBool(*field))
        {
            return *value;
        }
    }
    return true;
}

void AppendWarning(std::string& warnings, const std::string& message)
{
    if (!warnings.empty())
    {
        warnings += "\n";
    }
    warnings += message;
}

std::optional<std::string> FirstStringField(const BinObject& object, const std::initializer_list<const char*> candidates)
{
    if (const BinValue* field = FindFieldInsensitive(object, candidates))
    {
        return ValueToString(*field);
    }
    return std::nullopt;
}

std::optional<float> FirstFloatField(const BinObject& object, const std::initializer_list<const char*> candidates)
{
    if (const BinValue* field = FindFieldInsensitive(object, candidates))
    {
        return ValueToFloat(*field);
    }
    return std::nullopt;
}

std::optional<bool> FirstBoolField(const BinObject& object, const std::initializer_list<const char*> candidates)
{
    if (const BinValue* field = FindFieldInsensitive(object, candidates))
    {
        return ValueToBool(*field);
    }
    return std::nullopt;
}

std::optional<glm::vec3> FirstVec3Field(const BinObject& object, const std::initializer_list<const char*> candidates)
{
    if (const BinValue* field = FindFieldInsensitive(object, candidates))
    {
        return ValueToVec3(*field);
    }
    return std::nullopt;
}

std::optional<glm::vec4> FirstVec4Field(const BinObject& object, const std::initializer_list<const char*> candidates)
{
    if (const BinValue* field = FindFieldInsensitive(object, candidates))
    {
        return ValueToVec4(*field);
    }
    return std::nullopt;
}

const BinObject* FirstNestedObject(const BinObject& object, const std::initializer_list<const char*> fieldCandidates, const std::initializer_list<const char*> typeNeedles)
{
    if (const BinValue* field = FindFieldInsensitive(object, fieldCandidates))
    {
        if (const BinObjectPtr* nested = field->AsObject())
        {
            if (*nested != nullptr)
            {
                return nested->get();
            }
        }
    }

    std::vector<const BinObject*> matches = CollectObjectsByTypeRecursive(object, typeNeedles);
    for (const BinObject* match : matches)
    {
        if (match != &object)
        {
            return match;
        }
    }

    return nullptr;
}

std::unordered_map<std::string, std::string> BuildTextureLookup(const BinObject& root, const std::filesystem::path& sourcePath)
{
    std::unordered_map<std::string, std::string> textureLookup;
    const BinValue* texturesValue = FindFieldInsensitive(root, {"textures", "textureDefinitions", "textureRefs"});
    std::vector<const BinObject*> textures = CollectListObjectsByType(texturesValue, {"texture"});
    std::vector<const BinObject*> recursive = CollectObjectsByTypeRecursive(root, {"texture"});
    textures.insert(textures.end(), recursive.begin(), recursive.end());

    for (const BinObject* texture : textures)
    {
        const std::optional<std::string> name = FirstStringField(*texture, {"name", "textureName", "id"});
        const std::optional<std::string> path = FirstStringField(*texture, {"path", "texturePath", "sourcePath", "filePath", "file"});
        if (name && path)
        {
            textureLookup[*name] = ResolveRelativePath(sourcePath, *path);
        }
    }
    return textureLookup;
}

std::unordered_map<std::string, VfxMaterialRef> BuildMaterialLookup(
    const BinObject& root,
    const std::filesystem::path& sourcePath,
    const std::unordered_map<std::string, std::string>& textureLookup)
{
    std::unordered_map<std::string, VfxMaterialRef> materials;
    const BinValue* materialsValue = FindFieldInsensitive(root, {"materials", "materialDefinitions", "materialRefs"});
    std::vector<const BinObject*> materialObjects = CollectListObjectsByType(materialsValue, {"material"});
    std::vector<const BinObject*> recursive = CollectObjectsByTypeRecursive(root, {"material"});
    materialObjects.insert(materialObjects.end(), recursive.begin(), recursive.end());

    for (const BinObject* materialObject : materialObjects)
    {
        VfxMaterialRef material;
        material.name = FirstStringField(*materialObject, {"name", "materialName", "id"}).value_or("Material");

        std::optional<std::string> texturePath = FirstStringField(*materialObject, {"texturePath", "path", "filePath"});
        if (!texturePath)
        {
            const std::optional<std::string> textureRef = FirstStringField(*materialObject, {"texture", "textureName", "textureRef"});
            if (textureRef)
            {
                const auto resolved = textureLookup.find(*textureRef);
                if (resolved != textureLookup.end())
                {
                    texturePath = resolved->second;
                }
                else
                {
                    texturePath = *textureRef;
                }
            }
        }
        if (texturePath)
        {
            material.texturePath = ResolveRelativePath(sourcePath, *texturePath);
        }

        if (const std::optional<glm::vec4> tint = FirstVec4Field(*materialObject, {"tint", "color", "tintColor"}))
        {
            material.tint = *tint;
        }

        materials[material.name] = material;
    }
    return materials;
}

VfxEmitter ConvertEmitter(
    const BinObject& emitterObject,
    const std::filesystem::path& sourcePath,
    const std::unordered_map<std::string, VfxMaterialRef>& materialLookup,
    const std::unordered_map<std::string, std::string>& textureLookup)
{
    VfxEmitter emitter = DefaultEmitter();
    emitter.name = FirstStringField(emitterObject, {"name", "emitterName", "id"}).value_or(emitter.name);
    if (const std::optional<float> spawnRate = FirstFloatField(emitterObject, {"spawnRate", "rate", "particleSpawnRate"}))
    {
        emitter.spawnRate = *spawnRate;
    }
    if (const std::optional<float> burstCount = FirstFloatField(emitterObject, {"burstCount", "numBursts"}))
    {
        emitter.burstCount = *burstCount;
    }
    if (const std::optional<float> maxParticles = FirstFloatField(emitterObject, {"maxParticles", "particleMaximum"}))
    {
        emitter.maxParticles = std::max(1, static_cast<int>(*maxParticles));
    }
    if (const std::optional<bool> loop = FirstBoolField(emitterObject, {"loop", "isLooping", "looping"}))
    {
        emitter.loop = *loop;
    }
    if (const std::optional<glm::vec3> localOffset = FirstVec3Field(emitterObject, {"position", "localOffset", "offset"}))
    {
        emitter.localOffset = *localOffset;
    }
    if (const std::optional<glm::vec3> velocity = FirstVec3Field(emitterObject, {"velocity"}))
    {
        emitter.velocityMin = *velocity;
        emitter.velocityMax = *velocity;
    }
    if (const std::optional<glm::vec3> velocityMin = FirstVec3Field(emitterObject, {"velocityMin", "minVelocity"}))
    {
        emitter.velocityMin = *velocityMin;
    }
    if (const std::optional<glm::vec3> velocityMax = FirstVec3Field(emitterObject, {"velocityMax", "maxVelocity"}))
    {
        emitter.velocityMax = *velocityMax;
    }

    const BinObject* particleObject = FirstNestedObject(
        emitterObject,
        {"particle", "particleDefinition", "particleData"},
        {"particledefinition", "particle"}
    );

    const BinObject* lifetimeSource = particleObject != nullptr ? particleObject : &emitterObject;
    if (const std::optional<float> lifetime = FirstFloatField(*lifetimeSource, {"lifetime"}))
    {
        emitter.lifetimeMin = *lifetime;
        emitter.lifetimeMax = *lifetime;
    }
    if (const std::optional<float> lifetimeMin = FirstFloatField(*lifetimeSource, {"lifetimeMin", "lifetimeMinimum"}))
    {
        emitter.lifetimeMin = *lifetimeMin;
    }
    if (const std::optional<float> lifetimeMax = FirstFloatField(*lifetimeSource, {"lifetimeMax", "lifetimeMaximum"}))
    {
        emitter.lifetimeMax = *lifetimeMax;
    }
    if (const std::optional<float> startSize = FirstFloatField(*lifetimeSource, {"startSize", "size"}))
    {
        emitter.startSize = *startSize;
    }
    if (const std::optional<float> endSize = FirstFloatField(*lifetimeSource, {"endSize"}))
    {
        emitter.endSize = *endSize;
    }
    if (const std::optional<glm::vec4> startColor = FirstVec4Field(*lifetimeSource, {"startColor", "color"}))
    {
        emitter.startColor = *startColor;
    }
    if (const std::optional<glm::vec4> endColor = FirstVec4Field(*lifetimeSource, {"endColor"}))
    {
        emitter.endColor = *endColor;
    }

    if (const std::optional<std::string> materialName = FirstStringField(emitterObject, {"materialName", "material", "materialRef"}))
    {
        emitter.materialName = *materialName;
        const auto materialIterator = materialLookup.find(*materialName);
        if (materialIterator != materialLookup.end())
        {
            if (emitter.texturePath.empty())
            {
                emitter.texturePath = materialIterator->second.texturePath;
            }
            emitter.startColor *= materialIterator->second.tint;
            emitter.endColor *= materialIterator->second.tint;
        }
    }

    std::optional<std::string> texturePath = FirstStringField(*lifetimeSource, {"texturePath", "path", "filePath"});
    if (!texturePath)
    {
        texturePath = FirstStringField(*lifetimeSource, {"texture", "textureName", "textureRef"});
    }
    if (!texturePath)
    {
        texturePath = FirstStringField(emitterObject, {"texturePath", "texture", "textureName", "textureRef"});
    }
    if (texturePath)
    {
        const auto resolved = textureLookup.find(*texturePath);
        emitter.texturePath = ResolveRelativePath(sourcePath, resolved != textureLookup.end() ? resolved->second : *texturePath);
    }

    std::optional<std::string> blendMode = FirstStringField(emitterObject, {"blendMode", "blend", "materialBlendMode"});
    if (!blendMode && !emitter.materialName.empty())
    {
        const auto materialIterator = materialLookup.find(emitter.materialName);
        if (materialIterator != materialLookup.end())
        {
            blendMode = materialIterator->second.name;
        }
    }
    if (blendMode)
    {
        emitter.additiveBlending = !ContainsInsensitive(*blendMode, "alpha");
    }

    return emitter;
}

VfxCurve ConvertCurve(const BinObject& curveObject)
{
    VfxCurve curve;
    curve.name = FirstStringField(curveObject, {"name", "curveName", "id"}).value_or("Curve");
    if (const BinValue* pointsValue = FindFieldInsensitive(curveObject, {"points", "keys", "curvePoints"}))
    {
        if (const BinList* points = pointsValue->AsList())
        {
            for (const BinValue& pointValue : *points)
            {
                if (const BinObjectPtr* pointObject = pointValue.AsObject())
                {
                    if (*pointObject == nullptr)
                    {
                        continue;
                    }
                    const std::optional<float> x = FirstFloatField(**pointObject, {"x", "time", "t"});
                    const std::optional<float> y = FirstFloatField(**pointObject, {"y", "value", "v"});
                    if (x && y)
                    {
                        curve.points.emplace_back(*x, *y);
                    }
                }
                else if (const BinList* pair = pointValue.AsList())
                {
                    if (pair->size() >= 2)
                    {
                        const std::optional<float> x = ValueToFloat((*pair)[0]);
                        const std::optional<float> y = ValueToFloat((*pair)[1]);
                        if (x && y)
                        {
                            curve.points.emplace_back(*x, *y);
                        }
                    }
                }
            }
        }
    }
    return curve;
}

VfxChildEffect ConvertChild(const BinObject& childObject, const std::filesystem::path& sourcePath)
{
    VfxChildEffect child;
    child.name = FirstStringField(childObject, {"name", "effectName", "childName", "id"}).value_or("Child");
    if (const std::optional<std::string> path = FirstStringField(childObject, {"path", "filePath", "effectPath", "childPath"}))
    {
        child.path = ResolveRelativePath(sourcePath, *path);
    }
    return child;
}
}

std::optional<VfxGraph> BinToVfxGraph::Convert(const BinObject& root, const std::filesystem::path& sourcePath, std::string& warnings)
{
    warnings.clear();

    VfxGraph graph;
    graph.name = GraphNameFromRoot(root, sourcePath);
    graph.durationSeconds = GraphDurationFromRoot(root);
    graph.loop = GraphLoopFromRoot(root);

    const std::unordered_map<std::string, std::string> textureLookup = BuildTextureLookup(root, sourcePath);
    const std::unordered_map<std::string, VfxMaterialRef> materialLookup = BuildMaterialLookup(root, sourcePath, textureLookup);
    for (const auto& [name, material] : materialLookup)
    {
        (void)name;
        graph.materials.push_back(material);
    }

    std::vector<const BinObject*> emitters;
    const BinValue* emittersValue = FindFieldInsensitive(root, {"emitters", "emitterDefinitions", "systems"});
    std::vector<const BinObject*> explicitEmitters = CollectListObjectsByType(emittersValue, {"emitter"});
    emitters.insert(emitters.end(), explicitEmitters.begin(), explicitEmitters.end());

    if (emitters.empty())
    {
        std::vector<const BinObject*> recursiveEmitters = CollectObjectsByTypeRecursive(root, {"emitterdefinition", "emitter"});
        for (const BinObject* emitterObject : recursiveEmitters)
        {
            if (emitterObject != &root)
            {
                emitters.push_back(emitterObject);
            }
        }
    }

    for (const BinObject* emitterObject : emitters)
    {
        graph.emitters.push_back(ConvertEmitter(*emitterObject, sourcePath, materialLookup, textureLookup));
    }

    const BinValue* curvesValue = FindFieldInsensitive(root, {"curves", "curveDefinitions"});
    std::vector<const BinObject*> explicitCurves = CollectListObjectsByType(curvesValue, {"curve"});
    for (const BinObject* curveObject : explicitCurves)
    {
        graph.curves.push_back(ConvertCurve(*curveObject));
    }

    const BinValue* childrenValue = FindFieldInsensitive(root, {"children", "childEffects", "childSystems"});
    std::vector<const BinObject*> explicitChildren = CollectListObjectsByType(childrenValue, {"child", "effect"});
    for (const BinObject* childObject : explicitChildren)
    {
        graph.children.push_back(ConvertChild(*childObject, sourcePath));
    }

    if (graph.emitters.empty())
    {
        AppendWarning(warnings, "No VFX emitters mapped from BIN AST.");
        return std::nullopt;
    }

    if (!ContainsInsensitive(root.typeName, "vfx") && !ContainsInsensitive(root.typeName, "effect"))
    {
        AppendWarning(warnings, "Root BIN object type does not look VFX-specific: " + root.typeName);
    }

    if (graph.materials.empty())
    {
        AppendWarning(warnings, "No material definitions were mapped from BIN AST.");
    }

    return graph;
}
}
