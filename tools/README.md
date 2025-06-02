# SPZ Command-Line Tools

This directory contains command-line tools for working with the SPZ format.

## spz-convert

A command-line tool for converting .ply or .splat files to the compressed .spz format.

### Usage

```bash
spz-convert [OPTIONS] <input_file> <output_file>
```

### Arguments

- `input_file`: Input file (.ply or .splat)
- `output_file`: Output file (.spz)

### Options

- `-h, --help`: Show help message
- `--from-coord SYSTEM`: Input coordinate system (default: auto-detect)
- `--to-coord SYSTEM`: Output coordinate system (default: RUB)
- `--antialiased`: Mark output as antialiased

### Coordinate Systems

- `RDF`: Right Down Front (PLY default)
- `RUB`: Right Up Back (SPZ default, Three.js)
- `LUF`: Left Up Front (GLB)
- `RUF`: Right Up Front (Unity)
- `LDB`: Left Down Back
- `RDB`: Right Down Back
- `LUB`: Left Up Back
- `LDF`: Left Down Front

### Examples

Convert a PLY file to SPZ:
```bash
spz-convert input.ply output.spz
```

Convert a .splat file to SPZ:
```bash
spz-convert input.splat output.spz
```

Convert with antialiasing flag:
```bash
spz-convert --antialiased input.ply output.spz
```

Convert with explicit coordinate system conversion:
```bash
spz-convert --from-coord RDF --to-coord RUB input.ply output.spz
```

### Supported Input Formats

#### PLY Files (.ply)
Standard PLY files containing Gaussian splat data with the following properties:
- Position: `x`, `y`, `z`
- Scale: `scale_0`, `scale_1`, `scale_2`
- Rotation: `rot_0`, `rot_1`, `rot_2`, `rot_3` (quaternion)
- Opacity: `opacity`
- Color: `f_dc_0`, `f_dc_1`, `f_dc_2` (spherical harmonics DC component)
- Spherical harmonics: `f_rest_0`, `f_rest_1`, ... (optional)

#### SPLAT Files (.splat)
Binary files with 32-byte records containing:
- Position: 3 × 4 bytes (float)
- Scale: 3 × 4 bytes (float)
- Color: 4 bytes (RGBA, uint8)
- Rotation: 4 bytes (quaternion as uint8, normalized)

### Building

The tool is built automatically when you build the SPZ library with CMake:

```bash
mkdir build
cd build
cmake ..
make
```

To disable building tools:
```bash
cmake -DBUILD_TOOLS=OFF ..
```

### Output

The tool provides information about the conversion process:
- Number of points loaded
- Spherical harmonics degree
- File size comparison
- Compression ratio

SPZ files are typically 10x smaller than equivalent PLY files with minimal visual differences. 