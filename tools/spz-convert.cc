#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "load-spz.h"
#include "splat-types.h"

// Helper function to compute inverse sigmoid (logit)
float invSigmoid(float x) {
  x = std::max(1e-6f, std::min(x, 1.0f - 1e-6f));
  return std::log(x / (1.0f - x));
}

void printUsage(const char *programName) {
  std::cout << "Usage: " << programName
            << " [OPTIONS] <input_file> <output_file>\n\n";
  std::cout << "Convert .ply or .splat files to .spz format\n\n";
  std::cout << "Arguments:\n";
  std::cout << "  input_file   Input file (.ply or .splat)\n";
  std::cout << "  output_file  Output file (.spz)\n\n";
  std::cout << "Options:\n";
  std::cout << "  -h, --help              Show this help message\n";
  std::cout << "  --from-coord SYSTEM     Input coordinate system (default: "
               "auto-detect)\n";
  std::cout
      << "  --to-coord SYSTEM       Output coordinate system (default: RUB)\n";
  std::cout << "  --antialiased           Mark output as antialiased\n\n";
  std::cout << "Coordinate systems:\n";
  std::cout << "  RDF  Right Down Front (PLY default)\n";
  std::cout << "  RUB  Right Up Back (SPZ default, Three.js)\n";
  std::cout << "  LUF  Left Up Front (GLB)\n";
  std::cout << "  RUF  Right Up Front (Unity)\n";
  std::cout << "  LDB  Left Down Back\n";
  std::cout << "  RDB  Right Down Back\n";
  std::cout << "  LUB  Left Up Back\n";
  std::cout << "  LDF  Left Down Front\n\n";
  std::cout << "Examples:\n";
  std::cout << "  " << programName << " input.ply output.spz\n";
  std::cout << "  " << programName << " input.splat output.spz\n";
  std::cout << "  " << programName << " --antialiased input.ply output.spz\n";
  std::cout << "  " << programName
            << " --from-coord RDF --to-coord RUB input.ply output.spz\n";
}

spz::CoordinateSystem parseCoordinateSystem(const std::string &str) {
  if (str == "RDF")
    return spz::CoordinateSystem::RDF;
  if (str == "RUB")
    return spz::CoordinateSystem::RUB;
  if (str == "LUF")
    return spz::CoordinateSystem::LUF;
  if (str == "RUF")
    return spz::CoordinateSystem::RUF;
  if (str == "LDB")
    return spz::CoordinateSystem::LDB;
  if (str == "RDB")
    return spz::CoordinateSystem::RDB;
  if (str == "LUB")
    return spz::CoordinateSystem::LUB;
  if (str == "LDF")
    return spz::CoordinateSystem::LDF;
  return spz::CoordinateSystem::UNSPECIFIED;
}

std::string getFileExtension(const std::string &filename) {
  size_t pos = filename.find_last_of('.');
  if (pos == std::string::npos)
    return "";
  return filename.substr(pos);
}

// Load .splat file format (simple binary format with 32-byte records)
// Each record contains: position(3*float), scale(3*float), color(4*uint8),
// rotation(4*uint8)
spz::GaussianCloud loadSplatFile(const std::string &filename,
                                 const spz::UnpackOptions &options) {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    std::cerr << "Error: Unable to open file: " << filename << std::endl;
    return {};
  }

  size_t fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  // Each splat record is typically 32 bytes:
  // - position: 3 * 4 bytes (float) = 12 bytes
  // - scale: 3 * 4 bytes (float) = 12 bytes
  // - color: 4 bytes (RGBA, uint8)
  // - rotation: 4 bytes (quaternion as uint8, normalized)
  const size_t recordSize = 32;

  if (fileSize % recordSize != 0) {
    std::cerr << "Error: Invalid .splat file size. Expected multiple of "
              << recordSize << " bytes." << std::endl;
    return {};
  }

  int32_t numPoints = fileSize / recordSize;
  std::cout << "Loading " << numPoints << " points from .splat file"
            << std::endl;

  spz::GaussianCloud cloud;
  cloud.numPoints = numPoints;
  cloud.shDegree = 0; // .splat files typically don't have spherical harmonics
  cloud.antialiased = false;

  cloud.positions.reserve(numPoints * 3);
  cloud.scales.reserve(numPoints * 3);
  cloud.rotations.reserve(numPoints * 4);
  cloud.alphas.reserve(numPoints);
  cloud.colors.reserve(numPoints * 3);

  for (int32_t i = 0; i < numPoints; i++) {
    // Read position (3 floats)
    float pos[3];
    file.read(reinterpret_cast<char *>(pos), 3 * sizeof(float));
    cloud.positions.push_back(pos[0]);
    cloud.positions.push_back(pos[1]);
    cloud.positions.push_back(pos[2]);

    // Read scale (3 floats, convert to log scale)
    float scale[3];
    file.read(reinterpret_cast<char *>(scale), 3 * sizeof(float));
    cloud.scales.push_back(std::log(std::max(scale[0], 1e-8f)));
    cloud.scales.push_back(std::log(std::max(scale[1], 1e-8f)));
    cloud.scales.push_back(std::log(std::max(scale[2], 1e-8f)));

    // Read color (4 uint8: RGBA)
    uint8_t color[4];
    file.read(reinterpret_cast<char *>(color), 4);
    // Convert from [0,255] to SH DC component format
    cloud.colors.push_back((color[0] / 255.0f - 0.5f) / 0.282095f);
    cloud.colors.push_back((color[1] / 255.0f - 0.5f) / 0.282095f);
    cloud.colors.push_back((color[2] / 255.0f - 0.5f) / 0.282095f);

    // Alpha from the 4th color component
    cloud.alphas.push_back(invSigmoid(color[3] / 255.0f));

    // Read rotation (4 uint8, convert to normalized quaternion)
    uint8_t rot[4];
    file.read(reinterpret_cast<char *>(rot), 4);
    // Convert from [0,255] to [-1,1] and normalize
    float qx = (rot[0] / 255.0f) * 2.0f - 1.0f;
    float qy = (rot[1] / 255.0f) * 2.0f - 1.0f;
    float qz = (rot[2] / 255.0f) * 2.0f - 1.0f;
    float qw = (rot[3] / 255.0f) * 2.0f - 1.0f;

    // Normalize quaternion
    float norm = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (norm > 0) {
      qx /= norm;
      qy /= norm;
      qz /= norm;
      qw /= norm;
    } else {
      qx = qy = qz = 0.0f;
      qw = 1.0f;
    }

    cloud.rotations.push_back(qx);
    cloud.rotations.push_back(qy);
    cloud.rotations.push_back(qz);
    cloud.rotations.push_back(qw);
  }

  if (!file.good()) {
    std::cerr << "Error: Failed to read .splat file completely" << std::endl;
    return {};
  }

  // Apply coordinate system conversion
  cloud.convertCoordinates(spz::CoordinateSystem::RUB, options.to);

  return cloud;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  std::string inputFile;
  std::string outputFile;
  spz::CoordinateSystem fromCoord = spz::CoordinateSystem::UNSPECIFIED;
  spz::CoordinateSystem toCoord = spz::CoordinateSystem::RUB;
  bool antialiased = false;

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "--from-coord") {
      if (i + 1 >= argc) {
        std::cerr << "Error: --from-coord requires a coordinate system argument"
                  << std::endl;
        return 1;
      }
      fromCoord = parseCoordinateSystem(argv[++i]);
      if (fromCoord == spz::CoordinateSystem::UNSPECIFIED) {
        std::cerr << "Error: Invalid coordinate system: " << argv[i]
                  << std::endl;
        return 1;
      }
    } else if (arg == "--to-coord") {
      if (i + 1 >= argc) {
        std::cerr << "Error: --to-coord requires a coordinate system argument"
                  << std::endl;
        return 1;
      }
      toCoord = parseCoordinateSystem(argv[++i]);
      if (toCoord == spz::CoordinateSystem::UNSPECIFIED) {
        std::cerr << "Error: Invalid coordinate system: " << argv[i]
                  << std::endl;
        return 1;
      }
    } else if (arg == "--antialiased") {
      antialiased = true;
    } else if (arg.substr(0, 1) == "-") {
      std::cerr << "Error: Unknown option: " << arg << std::endl;
      return 1;
    } else {
      if (inputFile.empty()) {
        inputFile = arg;
      } else if (outputFile.empty()) {
        outputFile = arg;
      } else {
        std::cerr << "Error: Too many arguments" << std::endl;
        return 1;
      }
    }
  }

  if (inputFile.empty() || outputFile.empty()) {
    std::cerr << "Error: Both input and output files must be specified"
              << std::endl;
    printUsage(argv[0]);
    return 1;
  }

  // Check if input file exists
  if (!std::filesystem::exists(inputFile)) {
    std::cerr << "Error: Input file does not exist: " << inputFile << std::endl;
    return 1;
  }

  // Determine input file type and auto-detect coordinate system if not
  // specified
  std::string inputExt = getFileExtension(inputFile);
  std::transform(inputExt.begin(), inputExt.end(), inputExt.begin(), ::tolower);

  if (fromCoord == spz::CoordinateSystem::UNSPECIFIED) {
    if (inputExt == ".ply") {
      fromCoord = spz::CoordinateSystem::RDF;
    } else if (inputExt == ".splat") {
      fromCoord = spz::CoordinateSystem::RUB; // Assume .splat uses RUB
    } else {
      std::cerr << "Error: Unknown input file type: " << inputExt << std::endl;
      std::cerr << "Supported formats: .ply, .splat" << std::endl;
      return 1;
    }
  }

  // Load the input file
  spz::GaussianCloud cloud;
  spz::UnpackOptions unpackOptions;
  unpackOptions.to = toCoord;

  std::cout << "Loading " << inputFile << "..." << std::endl;

  if (inputExt == ".ply") {
    cloud = spz::loadSplatFromPly(inputFile, unpackOptions);
  } else if (inputExt == ".splat") {
    cloud = loadSplatFile(inputFile, unpackOptions);
  } else {
    std::cerr << "Error: Unsupported input file format: " << inputExt
              << std::endl;
    return 1;
  }

  if (cloud.numPoints == 0) {
    std::cerr << "Error: Failed to load input file or file contains no points"
              << std::endl;
    return 1;
  }

  // Set antialiased flag if requested
  cloud.antialiased = antialiased;

  std::cout << "Loaded " << cloud.numPoints << " points" << std::endl;
  std::cout << "Spherical harmonics degree: " << cloud.shDegree << std::endl;
  std::cout << "Antialiased: " << (cloud.antialiased ? "yes" : "no")
            << std::endl;

  // Save to SPZ format
  std::cout << "Saving to " << outputFile << "..." << std::endl;

  spz::PackOptions packOptions;
  packOptions.from = toCoord; // Data is already in target coordinate system

  bool success = spz::saveSpz(cloud, packOptions, outputFile);

  if (!success) {
    std::cerr << "Error: Failed to save SPZ file" << std::endl;
    return 1;
  }

  // Show file size comparison
  size_t inputSize = std::filesystem::file_size(inputFile);
  size_t outputSize = std::filesystem::file_size(outputFile);
  double compressionRatio = static_cast<double>(inputSize) / outputSize;

  std::cout << "Conversion completed successfully!" << std::endl;
  std::cout << "Input size: " << inputSize << " bytes" << std::endl;
  std::cout << "Output size: " << outputSize << " bytes" << std::endl;
  std::cout << "Compression ratio: " << std::fixed << std::setprecision(1)
            << compressionRatio << "x" << std::endl;

  return 0;
}