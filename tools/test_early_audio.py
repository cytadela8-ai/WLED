"""Compile and run actual AudioReactive state methods against host hardware boundaries.

Extract the two methods without copying their implementation into tests. The fixture
uses WLED's vendored ArduinoJson; task and palette side effects are test counters.
No ESP32 toolchain or additional Python packages are needed.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile


def extract_method(source: str, signature: str) -> str:
    """Extract a known method whose body has balanced braces, including comments."""
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end]


def main() -> None:
    """Build the firmware methods into the host fixture and run its assertions."""
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-file",
        type=Path,
        default=root / "usermods/audioreactive/audio_reactive.cpp",
    )
    args = parser.parse_args()
    source = args.source_file.read_text()
    methods = [
        extract_method(source, "bool getUMData(um_data_t **data) override"),
        extract_method(source, "void readFromJsonState(JsonObject& root) override"),
    ]
    with tempfile.TemporaryDirectory(prefix="wled-early-audio-") as directory:
        directory = Path(directory)
        (directory / "audio_methods.h").write_text("\n".join(methods))
        executable = directory / "audio-state-test"
        subprocess.run(
            [
                "g++",
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(root),
                "-I",
                str(directory),
                str(root / "tests/early_boot/audio_state.cpp"),
                "-o",
                str(executable),
            ],
            check=True,
        )
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
