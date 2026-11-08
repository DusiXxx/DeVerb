#pragma once

#include <vector>
#include <utility>

// Tovarni presety. Hodnoty jsou ve skutecnych jednotkach (ms, %, Hz, s, dB),
// u vyberu poradi polozky, u prepinacu 0/1. Co neni uvedeno, dostane vychozi hodnotu.
// Nazvy a kategorie jsou v UTF-8.
struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<FactoryPreset>& factoryPresets();
const std::vector<const char*>& presetCategories();
