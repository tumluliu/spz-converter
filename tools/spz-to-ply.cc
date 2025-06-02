#include <iostream>
#include <string>

#include "load-spz.h"
#include "splat-types.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <input.spz> <output.ply>"
              << std::endl;
    return 1;
  }

  std::string inputFile = argv[1];
  std::string outputFile = argv[2];

  std::cout << "Loading " << inputFile << "..." << std::endl;

  spz::UnpackOptions unpackOptions;
  unpackOptions.to =
      spz::CoordinateSystem::RDF; // Convert to PLY coordinate system

  spz::GaussianCloud cloud = spz::loadSpz(inputFile, unpackOptions);

  if (cloud.numPoints == 0) {
    std::cerr << "Error: Failed to load SPZ file or file contains no points"
              << std::endl;
    return 1;
  }

  std::cout << "Loaded " << cloud.numPoints << " points" << std::endl;

  spz::PackOptions packOptions;
  packOptions.from =
      spz::CoordinateSystem::RDF; // Data is in PLY coordinate system

  bool success = spz::saveSplatToPly(cloud, packOptions, outputFile);

  if (!success) {
    std::cerr << "Error: Failed to save PLY file" << std::endl;
    return 1;
  }

  std::cout << "Conversion completed successfully!" << std::endl;
  return 0;
}