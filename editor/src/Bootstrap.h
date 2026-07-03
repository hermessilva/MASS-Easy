#pragma once
// Bridge between the legacy MASS training files and the unified .mass project.
//  - Bootstrap: human.xml + muscle284.xml + metadata.txt  ->  Model
//  - Export:    Model  ->  human.xml + muscle284.xml + metadata.txt
#include "MassModel.h"
#include <string>

namespace mass {

// Build a Model from existing training assets. Any path may be empty to skip.
// data_root is prepended to the metadata's relative skel/muscle/bvh paths.
std::optional<Model> BootstrapFromLegacy(const std::string& metadata_path,
                                         const std::string& data_root,
                                         std::string* err = nullptr);

// Decompose a Model into training files inside out_dir.
// Writes human.xml, muscle284.xml and metadata.txt (bvh files are referenced, not copied).
bool ExportToLegacy(const Model& m, const std::string& out_dir, std::string* err = nullptr);

// Import reference muscle Hill parameters + attachments from an OpenSim .osim model.
std::vector<AtlasEntry> ImportOsimMuscles(const std::string& osim_path, std::string* err = nullptr);

} // namespace mass
