#!/usr/bin/env python3
"""Embed Godot class reference XML as compressed C++ data.

Reads all .xml files from a directory, zlib-compresses them, and writes
a doc_data.gen.cpp that godot-cpp's DocDataRegistration can load at runtime.

This replicates the logic from godot-cpp/tools/godotcpp.py::make_doc_source
without the SCons dependency.
"""
import sys
import zlib
from pathlib import Path


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <doc_classes_dir> <output_cpp>")
        sys.exit(1)

    doc_dir = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # Concatenate all XML files
    buf = ""
    for xml_file in sorted(doc_dir.glob("*.xml")):
        buf += xml_file.read_text(encoding="utf-8")

    raw = buf.encode("utf-8")
    decomp_size = len(raw)
    compressed = zlib.compress(raw, zlib.Z_BEST_COMPRESSION)
    comp_hash = hash(compressed)

    # Write C++ source
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n\n")
        f.write("#include <godot_cpp/godot.hpp>\n\n")
        f.write(f'static const char *_doc_data_hash = "{comp_hash}";\n')
        f.write(f"static const int _doc_data_uncompressed_size = {decomp_size};\n")
        f.write(f"static const int _doc_data_compressed_size = {len(compressed)};\n")
        f.write("static const unsigned char _doc_data_compressed[] = {\n")
        for i in range(len(compressed)):
            f.write(f"\t{compressed[i]},\n")
        f.write("};\n\n")
        f.write("static godot::internal::DocDataRegistration _doc_data_registration(")
        f.write("_doc_data_hash, _doc_data_uncompressed_size, ")
        f.write("_doc_data_compressed_size, _doc_data_compressed);\n")

    print(f"Generated {out_path}")
    print(f"  {len(list(doc_dir.glob('*.xml')))} XML files")
    print(f"  {decomp_size} bytes uncompressed → {len(compressed)} bytes compressed")


if __name__ == "__main__":
    main()
